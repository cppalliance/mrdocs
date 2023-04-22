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

#include "FrontendAction.hpp"
#include "Bitcode.hpp"
#include "CorpusImpl.hpp"
#include "meta/Reduce.hpp"
#include <mrdox/Error.hpp>
#include <mrdox/Metadata.hpp>
#include <clang/Tooling/CommonOptionsParser.h>
#include <llvm/Bitstream/BitstreamReader.h>
#include <llvm/Support/Mutex.h>
#include <cassert>

namespace clang {
namespace mrdox {

// A standalone function to call to merge a vector of infos into one.
// This assumes that all infos in the vector are of the same type, and will fail
// if they are different.
// Dispatch function.
llvm::Expected<std::unique_ptr<Info>>
mergeInfos(std::vector<std::unique_ptr<Info>>& Values)
{
    if (Values.empty() || !Values[0])
        return llvm::createStringError(llvm::inconvertibleErrorCode(),
            "no info values to merge");

    switch (Values[0]->IT) {
    case InfoType::IT_namespace:
        return reduce<NamespaceInfo>(Values);
    case InfoType::IT_record:
        return reduce<RecordInfo>(Values);
    case InfoType::IT_enum:
        return reduce<EnumInfo>(Values);
    case InfoType::IT_function:
        return reduce<FunctionInfo>(Values);
    case InfoType::IT_typedef:
        return reduce<TypedefInfo>(Values);
    default:
        return llvm::createStringError(llvm::inconvertibleErrorCode(),
            "unexpected info type");
    }
}

//------------------------------------------------
//
// Observers
//
//------------------------------------------------

/** Return the metadata for the global namespace.
*/
NamespaceInfo const&
Corpus::
globalNamespace() const noexcept
{
    return get<NamespaceInfo>(globalNamespaceID);
}

//------------------------------------------------

void
Corpus::
visit(SymbolID id, Visitor& f) const
{
    visit(get<Info>(id), f);
}

void
Corpus::
visit(Scope const& I, Visitor& f) const
{
    for(auto const& ref : I.Namespaces)
        visit(get<NamespaceInfo>(ref.USR), f);
    for(auto const& ref : I.Records)
        visit(get<RecordInfo>(ref.USR), f);
    for(auto const& ref : I.Functions)
        visit(get<FunctionInfo>(ref.USR), f);
    for(auto const& J : I.Typedefs)
        visit(J, f);
    for(auto const& J : I.Enums)
        visit(J, f);
}

void
Corpus::
visitWithOverloads(
    Scope const& I, Visitor& f) const
{
    for(auto const& ref : I.Namespaces)
        visit(get<NamespaceInfo>(ref.USR), f);
    for(auto const& ref : I.Records)
        visit(get<RecordInfo>(ref.USR), f);
    if(I.isNamespaceScope)
    {
        // VFALCO Should this be AS_public
        auto const set = makeOverloadsSet(
            *this, I, AccessSpecifier::AS_none);
        for(auto const& functionOverloads : set.list)
            f.visit(functionOverloads);
    }
    else
    {
        {
            auto const& set = makeOverloadsSet(
                *this, I, AccessSpecifier::AS_public);
            for(auto const& functionOverloads : set.list)
                f.visit(functionOverloads);
        }
        {
            auto const& set = makeOverloadsSet(
                *this, I, AccessSpecifier::AS_protected);
            for(auto const& functionOverloads : set.list)
                f.visit(functionOverloads);
        }
        {
            auto const& set = makeOverloadsSet(
                *this, I, AccessSpecifier::AS_private);
            for(auto const& functionOverloads : set.list)
                f.visit(functionOverloads);
        }
    }
    for(auto const& J : I.Typedefs)
        visit(J, f);
    for(auto const& J : I.Enums)
        visit(J, f);
}

void
Corpus::
visit(Info const& I, Visitor& f) const
{
    switch(I.IT)
    {
    case InfoType::IT_namespace:
        return f.visit(static_cast<NamespaceInfo const&>(I));
    case InfoType::IT_record:
        return f.visit(static_cast<RecordInfo const&>(I));
    case InfoType::IT_function:
        return f.visit(static_cast<FunctionInfo const&>(I));
    case InfoType::IT_typedef:
        return f.visit(static_cast<TypedefInfo const&>(I));
    case InfoType::IT_enum:
        return f.visit(static_cast<EnumInfo const&>(I));
    default:
        llvm_unreachable("wrong InfoType for viist");
    }
}

//------------------------------------------------
//
// Modifiers
//
//------------------------------------------------

llvm::Expected<std::unique_ptr<Corpus>>
Corpus::
build(
    tooling::ToolExecutor& ex,
    std::shared_ptr<Config const> config,
    Reporter& R)
{
    auto corpus = std::make_unique<CorpusImpl>(std::move(config));

    // Traverse the AST for all translation units
    // and emit serializd bitcode into tool results.
    // This operation happens ona thread pool.
    if(corpus->config()->verbose())
        R.print("Mapping declarations");
    if(auto err = ex.execute(
        makeFrontendActionFactory(
            *ex.getExecutionContext(), *corpus->config(), R),
        corpus->config()->ArgAdjuster))
    {
        if(! corpus->config()->IgnoreMappingFailures)
            return err;
        R.print("warning: mapping failed because ", toString(std::move(err)));
    }

    // Collect the symbols. Each symbol will have
    // a vector of one or more bitcodes. These will
    // be merged later.
    if(corpus->config()->verbose())
        R.print("Collecting symbols");
    auto bitcodes = collectBitcodes(ex);

    // First reducing phase (reduce all decls into one info per decl).
    if(corpus->config()->verbose())
        R.print("Reducing ", bitcodes.size(), " declarations");
    std::atomic<bool> GotFailure;
    GotFailure = false;
    corpus->config()->parallelForEach(
        bitcodes,
        [&](auto& Group)
        {
            // One or more Info for the same symbol ID
            std::vector<std::unique_ptr<Info>> Infos;

            // Each Bitcode can have multiple Infos
            for (auto& bitcode : Group.getValue())
            {
                auto infos = readBitcode(bitcode, R);
                if(R.error(infos, "read bitcode"))
                {
                    GotFailure = true;
                    return;
                }
                std::move(
                    infos->begin(),
                    infos->end(),
                    std::back_inserter(Infos));
            }

            auto merged = mergeInfos(Infos);
            if(R.error(merged, "merge metadata"))
            {
                GotFailure = true;
                return;
            }

            std::unique_ptr<Info> I(merged.get().release());
            assert(Group.getKey() == llvm::toStringRef(I->USR));
            corpus->insert(std::move(I));
        });

    if(corpus->config()->verbose())
        R.print("Collected ", corpus->InfoMap.size(), " symbols.\n");

    if(GotFailure)
        return makeErrorString("one or more errors occurred");

    //
    // Finish up
    //

    if(! corpus->canonicalize(R))
        return makeError("canonicalization failed");

    return corpus;
}

} // mrdox
} // clang
