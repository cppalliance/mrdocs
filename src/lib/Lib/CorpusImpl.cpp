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
            const CorpusImpl* impl =
                static_cast<const CorpusImpl*>(corpus);
            auto it = impl->info_.find(val->id);
            return (++it)->get();
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
    report::Level reportLevel,
    std::shared_ptr<ConfigImpl const> config,
    tooling::CompilationDatabase const& compilations)
{
    using clock_type = std::chrono::steady_clock;
    const auto format_duration =
        [](clock_type::duration delta)
    {
        auto delta_ms = std::chrono::duration_cast<
            std::chrono::milliseconds>(delta).count();
        if(delta_ms < 1000)
            return fmt::format("{} ms", delta_ms);
        else
            return fmt::format("{:.02f} s",
                delta_ms / 1000.0);
    };
    auto start_time = clock_type::now();

    auto corpus = std::make_unique<CorpusImpl>(config);

    // Traverse the AST for all translation units
    // and emit serializd bitcode into tool results.
    // This operation happens ona thread pool.
    report::print(reportLevel, "Extracting declarations");

    #define USE_BITCODE

    #ifdef USE_BITCODE
        BitcodeExecutionContext context(*config);
    #else
        InfoExecutionContext context(*config);
    #endif
    std::unique_ptr<tooling::FrontendActionFactory> action =
        makeFrontendActionFactory(context, *config);
    MRDOCS_ASSERT(action);

    // Get a copy of the filename strings
    std::vector<std::string> files =
        compilations.getAllFiles();

    if(files.empty())
        return Unexpected(formatError("Compilations database is empty"));

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

            // suppress error messages from the tool
            Tool.setPrintErrorMessage(false);

            if(Tool.run(action.get()))
                formatError("Failed to run action on {}", path).Throw();
        };

    // Run the action on all files in the database
    std::vector<Error> errors;
    if(files.size() == 1)
    {
        try
        {
            processFile(std::move(files.front()));
        }
        catch(Exception const& ex)
        {
            errors.push_back(ex.error());
        }
    }
    else
    {
        TaskGroup taskGroup(config->threadPool());
        for(std::size_t index = 0;
            std::string& file : files)
        {
            taskGroup.async(
            [&, idx = ++index, path = std::move(file)]()
            {
                report::format(reportLevel,
                    "[{}/{}] \"{}\"", idx, files.size(), path);

                processFile(std::move(path));
            });
        }
        errors = taskGroup.wait();
    }

    // Report warning and error totals
    context.reportEnd(reportLevel);

    if(! errors.empty())
    {
        Error err(errors);
        if(! (*config)->ignoreFailures)
            return Unexpected(err);
        report::warn(
            "Warning: mapping failed because ", err);
    }

    #ifdef USE_BITCODE
        report::format(reportLevel,
            "Extracted {} declarations in {}",
            context.getBitcode().size(),
            format_duration(clock_type::now() - start_time));
        start_time = clock_type::now();

        // First reducing phase (reduce all decls into one info per decl).
        report::format(reportLevel,
            "Reducing declarations");

        auto results = context.results();
        if(! results)
            return Unexpected(results.error());
        corpus->info_ = std::move(results.value());

        report::format(reportLevel,
            "Reduced {} symbols in {}",
                corpus->info_.size(),
                format_duration(clock_type::now() - start_time));
    #else
        auto results = context.results();
        if(! results)
            return Unexpected(results.error());
        corpus->info_ = std::move(results.value());

        report::format(reportLevel,
            "Extracted {} declarations in {}",
            corpus->info_.size(),
            format_duration(clock_type::now() - start_time));
    #endif

    return corpus;
}

} // mrdocs
} // clang
