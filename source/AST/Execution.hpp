//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

//===--- AllTUsExecution.h - Execute actions on all TUs. -*- C++ --------*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines a tool executor that runs given actions on all TUs in the
//  compilation database. Tool results are deuplicated by the result key.
//
//===----------------------------------------------------------------------===//

#ifndef MRDOX_TOOL_AST_EXECUTION_HPP
#define MRDOX_TOOL_AST_EXECUTION_HPP

#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/Execution.h>
#include <llvm/ADT/SmallString.h>
#include <optional>

namespace clang {
namespace mrdox {

/// Executes given frontend actions on all files/TUs in the compilation
/// database.
class Executor : public tooling::ToolExecutor
{
public:
    static const char* ExecutorName;

    /// Init with \p CompilationDatabase.
    /// This uses \p ThreadCount threads to exececute the actions on all files in
    /// parallel. If \p ThreadCount is 0, this uses `llvm::hardware_concurrency`.
    Executor(
        tooling::CompilationDatabase const& Compilations,
        llvm::StringRef workingDir,
        unsigned ThreadCount,
        std::shared_ptr<PCHContainerOperations> PCHContainerOps =
            std::make_shared<PCHContainerOperations>());

    /// Init with \p CommonOptionsParser. This is expected to be used by
    /// `createExecutorFromCommandLineArgs` based on commandline options.
    ///
    /// The executor takes ownership of \p Options.
    Executor(
        tooling::CommonOptionsParser Options,
        llvm::StringRef workingDir,
        unsigned ThreadCount,
        std::shared_ptr<PCHContainerOperations> PCHContainerOps =
            std::make_shared<PCHContainerOperations>());

    StringRef getExecutorName() const override
    {
        return ExecutorName;
    }

    using ToolExecutor::execute;

    llvm::Error
    execute(llvm::ArrayRef<std::pair<std::unique_ptr<
        tooling::FrontendActionFactory>,
        tooling::ArgumentsAdjuster>> Actions) override;

    tooling::ExecutionContext*
    getExecutionContext() override
    {
        return &Context;
    };

    tooling::ToolResults*
    getToolResults() override
    {
        return Results.get();
    }

    void mapVirtualFile(StringRef FilePath, StringRef Content) override
    {
        OverlayFiles[FilePath] = std::string(Content);
    }

private:
    // Used to store the parser when the executor is initialized with parser.
    std::optional<tooling::CommonOptionsParser> OptionsParser;
    tooling::CompilationDatabase const& Compilations;
    llvm::SmallString<240> workingDir_;
    std::unique_ptr<tooling::ToolResults> Results;
    tooling::ExecutionContext Context;
    llvm::StringMap<std::string> OverlayFiles;
    unsigned ThreadCount;
};

#if 0
extern llvm::cl::opt<unsigned> ExecutorConcurrency;
extern llvm::cl::opt<std::string> Filter;
#endif

} // mrdox
} // clang

#endif
