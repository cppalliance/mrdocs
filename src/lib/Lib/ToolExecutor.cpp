//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "ToolExecutor.hpp"
#include <mrdox/Support/Error.hpp>
#include <mrdox/Support/ThreadPool.hpp>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/VirtualFileSystem.h>

namespace clang {
namespace mrdox {

ToolExecutor::
ToolExecutor(
    report::Level reportLevel,
    Config const& config,
    tooling::CompilationDatabase const& Compilations)
    : reportLevel_(reportLevel)
    , config_(config)
    , Compilations(Compilations)
{
}

Error
ToolExecutor::
execute(
    std::unique_ptr<tooling::FrontendActionFactory> Action)
{
    if(! Action)
        return formatError("No action to execute.");

    // Get a copy of the filename strings
    std::vector<std::string> Files = Compilations.getAllFiles();

    auto const processFile =
    [&](std::string Path)
    {
        // Each thread gets an independent copy of a VFS to allow different
        // concurrent working directories.
        IntrusiveRefCntPtr<llvm::vfs::FileSystem> FS =
            llvm::vfs::createPhysicalFileSystem();

        // KRYSTIAN NOTE: ClangTool applies the SyntaxOnly, StripOutput,
        // and StripDependencyFile argument adjusters
        tooling::ClangTool Tool(Compilations, { Path },
            std::make_shared<PCHContainerOperations>(), FS);

        // suppress error messages from the tool
        Tool.setPrintErrorMessage(false);

        if(Tool.run(Action.get()))
            formatError("Failed to run action on {}", Path).Throw();
    };

    // Run the action on all files in the database
    std::vector<Error> errors;
    if(Files.size() > 1)
    {
        TaskGroup taskGroup(config_.threadPool());
        for(std::size_t Index = 0;
            std::string& File : Files)
        {
            taskGroup.async(
            [&, Idx = ++Index, Path = std::move(File)]()
            {
                report::format(reportLevel_,
                    "[{}/{}] \"{}\"", Idx, Files.size(), Path);

                processFile(std::move(Path));
            });
        }
        errors = taskGroup.wait();
    }
    else
    {
        try
        {
            processFile(std::move(Files.front()));
        }
        catch(Exception const& ex)
        {
            errors.push_back(ex.error());
        }
    }

    // Report warning and error totals
    Context.reportEnd(reportLevel_);

    if(! errors.empty())
        return Error(errors);

    return Error::success();
}

} // mrdox
} // clang
