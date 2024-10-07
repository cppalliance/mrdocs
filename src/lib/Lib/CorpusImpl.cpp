//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "CorpusImpl.hpp"
#include "lib/AST/ASTVisitor.hpp"
#include "lib/Metadata/Finalize.hpp"
#include "lib/Lib/Lookup.hpp"
#include "lib/Support/Error.hpp"
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Support/Error.hpp>
#include <llvm/ADT/STLExtras.h>
#include <chrono>

namespace clang {
namespace mrdocs {

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
        [](const Corpus* corpus, const Info* val) ->
            const Info*
        {
            MRDOCS_ASSERT(val);
            const CorpusImpl* impl =
                static_cast<const CorpusImpl*>(corpus);
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

namespace {
template <class Rep, class Period>
std::string
format_duration(
    std::chrono::duration<Rep, Period> delta)
{
    auto delta_ms = std::chrono::duration_cast<
        std::chrono::milliseconds>(delta).count();
    if (delta_ms < 1000)
    {
        return fmt::format("{} ms", delta_ms);
    }
    else
    {
        double const delta_s = static_cast<double>(delta_ms) / 1000.0;
        return fmt::format("{:.02f} s", delta_s);
    }
}
}

mrdocs::Expected<std::unique_ptr<Corpus>>
CorpusImpl::
build(
    report::Level reportLevel,
    std::shared_ptr<ConfigImpl const> const& config,
    tooling::CompilationDatabase const& compilations)
{
    using clock_type = std::chrono::steady_clock;
    auto start_time = clock_type::now();

    // ------------------------------------------
    // Create empty corpus
    // ------------------------------------------
    // The corpus will keep a reference to Config.
    std::unique_ptr<CorpusImpl> corpus = std::make_unique<CorpusImpl>(config);

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
    report::print(reportLevel, "Extracting declarations");

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
                report::log(reportLevel,
                    "[{}/{}] \"{}\"", idx, files.size(), path);
                processFile(path);
            });
        }
        errors = taskGroup.wait();
    }
    // Print diagnostics totals
    context.reportEnd(reportLevel);

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

    auto results = context.results();
    if(! results)
        return Unexpected(results.error());
    corpus->info_ = std::move(results.value());

    report::log(reportLevel,
        "Extracted {} declarations in {}",
        corpus->info_.size(),
        format_duration(clock_type::now() - start_time));

    // ------------------------------------------
    // Finalize corpus
    // ------------------------------------------
    auto lookup = std::make_unique<SymbolLookup>(*corpus);
    finalize(corpus->info_, *lookup);

    return corpus;
}

} // mrdocs
} // clang
