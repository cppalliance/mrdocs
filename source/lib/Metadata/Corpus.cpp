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

#include "AST/Bitcode.hpp"
#include "AST/FrontendAction.hpp"
#include "Metadata/CorpusImpl.hpp"
#include "Metadata/Reduce.hpp"
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

Corpus::~Corpus() noexcept = default;

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

Corpus::Visitor::~Visitor() noexcept = default;

bool
Corpus::
Visitor::
visit(
    NamespaceInfo const&)
{
    return true;
}

bool
Corpus::
Visitor::
visit(
    RecordInfo const&)
{
    return true;
}

bool
Corpus::
Visitor::
visit(
    Overloads const&)
{
    return true;
}

bool
Corpus::
Visitor::
visit(
    FunctionInfo const&)
{
    return true;
}

bool
Corpus::
Visitor::
visit(
    EnumInfo const&)
{
    return true;
}

bool
Corpus::
Visitor::
visit(
    TypedefInfo const&)
{
    return true;
}

//------------------------------------------------

bool
Corpus::
visit(SymbolID id, Visitor& f) const
{
    return visit(get<Info>(id), f);
}

bool
Corpus::
visit(Scope const& I, Visitor& f) const
{
    for(auto const& ref : I.Namespaces)
        if(! visit(get<NamespaceInfo>(ref.id), f))
            return false;
    for(auto const& ref : I.Records)
        if(! visit(get<RecordInfo>(ref.id), f))
            return false;
    for(auto const& ref : I.Functions)
        if(! visit(get<FunctionInfo>(ref.id), f))
            return false;
    for(auto const& J : I.Typedefs)
        if(! visit(J, f))
            return false;
    for(auto const& J : I.Enums)
        if(! visit(J, f))
            return false;
    return true;
}

bool
Corpus::
visitWithOverloads(
    Scope const& I, Visitor& f) const
{
    for(auto const& ref : I.Namespaces)
        if(! visit(get<NamespaceInfo>(ref.id), f))
            return false;
    for(auto const& ref : I.Records)
        if(! visit(get<RecordInfo>(ref.id), f))
            return false;
    if(I.isNamespaceScope)
    {
        // VFALCO Should this be AS_public
        auto const set = makeOverloadsSet(
            *this, I, AccessSpecifier::AS_none);
        for(auto const& functionOverloads : set.list)
            if(! f.visit(functionOverloads))
                return false;
    }
    else
    {
        {
            auto const& set = makeOverloadsSet(
                *this, I, AccessSpecifier::AS_public);
            for(auto const& functionOverloads : set.list)
                if(! f.visit(functionOverloads))
                    return false;
        }
        {
            auto const& set = makeOverloadsSet(
                *this, I, AccessSpecifier::AS_protected);
            for(auto const& functionOverloads : set.list)
                if(! f.visit(functionOverloads))
                    return false;
        }
        {
            auto const& set = makeOverloadsSet(
                *this, I, AccessSpecifier::AS_private);
            for(auto const& functionOverloads : set.list)
                if(! f.visit(functionOverloads))
                    return false;
        }
    }
    for(auto const& J : I.Typedefs)
        if(! visit(J, f))
            return false;
    for(auto const& J : I.Enums)
        if(! visit(J, f))
            return false;
    return true;
}

bool
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
            Assert(Group.getKey() == llvm::toStringRef(I->id));
            corpus->insert(std::move(I));
        });

    if(corpus->config()->verbose())
        R.print("Collected ", corpus->InfoMap.size(), " symbols.\n");

    if(GotFailure)
        return makeErrorString("one or more errors occurred");

    corpus->canonicalize(R);

    return corpus;
}

} // mrdox
} // clang
