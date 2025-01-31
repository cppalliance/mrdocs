//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "CorpusImpl.hpp"
#include "lib/AST/FrontendActionFactory.hpp"
#include "lib/Metadata/Finalize.hpp"
#include "lib/Lib/Lookup.hpp"
#include "lib/Support/Error.hpp"
#include "lib/Support/Chrono.hpp"
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/ThreadPool.hpp>
#include <chrono>

namespace clang::mrdocs {

auto
CorpusImpl::
begin() const noexcept ->
    iterator
{
    if(info_.empty())
        return end();
    // KRYSTIAN NOTE: this is far from ideal, but i'm not sure
    // to what extent implementation detail should be hidden.
    return iterator(this, info_.begin()->get(),
        [](Corpus const* corpus, Info const* val) ->
            Info const*
        {
            MRDOCS_ASSERT(val);
            CorpusImpl const* impl =
                static_cast<CorpusImpl const*>(corpus);
            auto it = impl->info_.find(val->id);
            if(++it == impl->info_.end())
                return nullptr;
            return it->get();
        });
}

auto
CorpusImpl::
end() const noexcept ->
    iterator
{
    return iterator();
}

Info*
CorpusImpl::
find(
    SymbolID const& id) noexcept
{
    auto it = info_.find(id);
    if(it != info_.end())
        return it->get();
    return nullptr;
}

Info const*
CorpusImpl::
find(
    SymbolID const& id) const noexcept
{
    auto it = info_.find(id);
    if(it != info_.end())
        return it->get();
    return nullptr;
}

//------------------------------------------------

mrdocs::Expected<std::unique_ptr<Corpus>>
CorpusImpl::
build(
    std::shared_ptr<ConfigImpl const> const& config,
    tooling::CompilationDatabase const& compilations)
{
    using clock_type = std::chrono::steady_clock;
    auto start_time = clock_type::now();

    // ------------------------------------------
    // Create empty corpus
    // ------------------------------------------
    // The corpus will keep a reference to Config.
    auto corpus = std::make_unique<CorpusImpl>(config);

    // ------------------------------------------
    // Execution context
    // ------------------------------------------
    // Create an execution context to store the
    // results of the AST traversal.
    // Any new Info objects will be added to the
    // InfoSet in the execution context.
    InfoExecutionContext context(*config);

    // Create an `ASTActionFactory` to create multiple
    // `ASTAction`s that extract the AST for each translation unit.
    std::unique_ptr<tooling::FrontendActionFactory> action =
        makeFrontendActionFactory(context, *config);
    MRDOCS_ASSERT(action);

    // ------------------------------------------
    // "Process file" task
    // ------------------------------------------
    auto const processFile =
        [&](std::string path)
        {
            // Each thread gets an independent copy of a VFS to allow different
            // concurrent working directories.
            IntrusiveRefCntPtr<llvm::vfs::FileSystem> FS =
                llvm::vfs::createPhysicalFileSystem();

            // KRYSTIAN NOTE: ClangTool applies the SyntaxOnly, StripOutput,
            // and StripDependencyFile argument adjusters
            tooling::ClangTool Tool(compilations, { path },
                std::make_shared<PCHContainerOperations>(), FS);

            // Suppress error messages from the tool
            Tool.setPrintErrorMessage(false);

            if (Tool.run(action.get()))
            {
                formatError("Failed to run action on {}", path).Throw();
            }
        };

    // ------------------------------------------
    // Run the process file task on all files
    // ------------------------------------------
    // Traverse the AST for all translation units.
    // This operation happens on a thread pool.
    report::info("Extracting declarations");

    // Get a copy of the filename strings
    std::vector<std::string> files = compilations.getAllFiles();
    MRDOCS_CHECK(files, "Compilations database is empty");
    std::vector<Error> errors;

    // Run the action on all files in the database
    if (files.size() == 1)
    {
        try
        {
            processFile(std::move(files.front()));
        }
        catch (Exception const& ex)
        {
            errors.push_back(ex.error());
        }
    }
    else
    {
        TaskGroup taskGroup(config->threadPool());
        std::size_t index = 0;
        for (std::string& file : files)
        {
            taskGroup.async(
            [&, idx = ++index, path = std::move(file)]()
            {
                report::debug("[{}/{}] \"{}\"", idx, files.size(), path);
                processFile(path);
            });
        }
        errors = taskGroup.wait();
    }
    // Print diagnostics totals
    context.reportEnd(report::Level::info);

    // ------------------------------------------
    // Report warning and error totals
    // ------------------------------------------
    if (!errors.empty())
    {
        Error err(errors);
        if (!(*config)->ignoreFailures)
        {
            return Unexpected(err);
        }
        report::warn(
            "Warning: mapping failed because ", err);
    }

    MRDOCS_TRY(auto results, context.results());
    corpus->info_ = std::move(results);

    report::info(
        "Extracted {} declarations in {}",
        corpus->info_.size(),
        format_duration(clock_type::now() - start_time));

    // ------------------------------------------
    // Finalize corpus
    // ------------------------------------------
    finalize(*corpus);

    return corpus;
}

} // clang::mrdocs
