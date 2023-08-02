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

#include "lib/AST/Bitcode.hpp"
#include "lib/AST/ASTVisitor.hpp"
#include "CorpusImpl.hpp"
#include "lib/Metadata/Reduce.hpp"
#include "lib/Support/Error.hpp"
#include <mrdox/Metadata.hpp>
#include <mrdox/Support/Error.hpp>
#include <llvm/ADT/STLExtras.h>

namespace clang {
namespace mrdox {

// A standalone function to call to merge a vector of infos into one.
// This assumes that all infos in the vector are of the same type, and will fail
// if they are different.
// Dispatch function.
llvm::Expected<std::unique_ptr<Info>>
mergeInfos(std::vector<std::unique_ptr<Info>>& Values)
{
    if(Values.empty() || ! Values[0])
        return llvm::createStringError(llvm::inconvertibleErrorCode(),
            "no info values to merge");

    return visit(*Values[0], [&]<typename T>(T&) mutable
        {
            return reduce<T>(Values);
        });
}

//------------------------------------------------

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

void
CorpusImpl::
insert(std::unique_ptr<Info> I)
{
    std::lock_guard<std::mutex> lock(mutex_);

    index_.emplace_back(I.get());

    // This has to come last because we move I.
    info_.emplace(std::move(I));
}

//------------------------------------------------

mrdox::Expected<std::unique_ptr<Corpus>>
CorpusImpl::
build(
    ToolExecutor& ex,
    std::shared_ptr<ConfigImpl const> config)
{
    auto corpus = std::make_unique<CorpusImpl>(config);

    // Traverse the AST for all translation units
    // and emit serializd bitcode into tool results.
    // This operation happens ona thread pool.
    report::print(ex.getReportLevel(), "Mapping declarations");
    if(Error err = ex.execute(
        makeFrontendActionFactory(
            ex.getExecutionContext(), *config)))
    {
        if(! (*config)->ignoreFailures)
            return Unexpected(err);
        report::warn(
            "Warning: mapping failed because ", err);
    }

    // Collect the symbols. Each symbol will have
    // a vector of one or more bitcodes. These will
    // be merged later.
    report::print(ex.getReportLevel(), "Collecting symbols");
    auto bitcodes = std::move(
        ex.getExecutionContext().getBitcode());

    // First reducing phase (reduce all decls into one info per decl).
    report::format(ex.getReportLevel(),
        "Reducing {} declarations", bitcodes.size());
    std::atomic<bool> GotFailure;
    GotFailure = false;
    auto errors = corpus->config.threadPool().forEach(
        bitcodes,
        [&](auto& Group)
        {
            // One or more Info for the same symbol ID
            std::vector<std::unique_ptr<Info>> Infos;

            // Each Bitcode can have multiple Infos
            for(auto& bitcode : Group.second)
            {
                auto infos = readBitcode(bitcode);
                if(! infos)
                {
                    report::error("{}: reading bitcode", infos.error());
                    GotFailure = true;
                    return;
                }
                std::move(
                    infos->begin(),
                    infos->end(),
                    std::back_inserter(Infos));
            }

            auto merged = mergeInfos(Infos);
            if(! merged)
            {
                report::error("{}: merging metadata", toError(merged.takeError()));
                GotFailure = true;
                return;
            }

            std::unique_ptr<Info> I(merged.get().release());
            MRDOX_ASSERT(Group.first == I->id);
            corpus->insert(std::move(I));
        });

    if(! errors.empty())
        return Unexpected(Error(errors));

    report::format(ex.getReportLevel(),
        "Symbols collected: {}", corpus->info_.size());

    if(GotFailure)
        return Unexpected(formatError("multiple errors occurred"));

    return corpus;
}

} // mrdox
} // clang
