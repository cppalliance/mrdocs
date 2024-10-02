//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "Lookup.hpp"
#include <mrdocs/Metadata.hpp>

namespace clang {
namespace mrdocs {

namespace {

bool supportsLookup(const Info* info)
{
    return info &&
        (info->isRecord() ||
        info->isNamespace() ||
        info->isEnum() ||
        info->isSpecialization());
}

bool isTransparent(const Info* info)
{
    MRDOCS_ASSERT(info);
    return visit(*info, []<typename InfoTy>(
        const InfoTy& I) -> bool
    {
        if constexpr(InfoTy::isNamespace())
            return I.IsInline;
        if constexpr(InfoTy::isEnum())
            return ! I.Scoped;
        return false;
    });
}

void
buildLookups(
    const Corpus& corpus,
    const Info& info,
    LookupTable& lookups)
{
    visit(info, [&]<typename InfoTy>(const InfoTy& I)
    {
        if constexpr(
            InfoTy::isRecord() ||
            InfoTy::isNamespace() ||
            InfoTy::isEnum())
        {
            for(const SymbolID& M : I.Members)
            {
                const Info* child = corpus.find(M);
                // if the member is an inline namespace or
                // an unscoped enumeration, add its members as well
                if(isTransparent(child))
                    buildLookups(corpus, *child, lookups);

                // KRYSTIAN TODO: handle inline/anonymous namespaces
                // KRYSTIAN TODO: injected class names?
                if(child->Name.empty())
                    continue;
                lookups.add(child->Name, child);
            }
        }
    });
}

} // (anon)

LookupTable::
LookupTable(
    const Info& info,
    const Corpus& corpus)
{
    MRDOCS_ASSERT(supportsLookup(&info));

    buildLookups(corpus, info, *this);
}

SymbolLookup::
SymbolLookup(const Corpus& corpus)
    : corpus_(corpus)
{
    for(const Info& I : corpus_)
    {
        if(! supportsLookup(&I))
            continue;
        lookup_tables_.emplace(&I, LookupTable(I, corpus_));
    }
}

const Info*
SymbolLookup::
adjustLookupContext(
    const Info* context)
{
    // find the innermost enclosing context that supports name lookup
    while(! supportsLookup(context))
    {
        MRDOCS_ASSERT(! context->Namespace.empty());
        const SymbolID& parent = context->Namespace.front();
        context = corpus_.find(parent);
    }
    return context;
}

const Info*
SymbolLookup::
lookThroughTypedefs(const Info* I)
{
    if(! I || ! I->isTypedef())
        return I;
    auto* TI = static_cast<const TypedefInfo*>(I);
    return lookThroughTypedefs(
        corpus_.find(TI->Type->namedSymbol()));
}

const Info*
SymbolLookup::
lookupInContext(
    const Info* context,
    std::string_view name,
    bool for_nns,
    LookupCallback& callback)
{
    // if the lookup context is a typedef, we want to
    // lookup the name in the type it denotes
    if(! (context = lookThroughTypedefs(context)))
        return nullptr;
    MRDOCS_ASSERT(supportsLookup(context));
    LookupTable& table = lookup_tables_.at(context);
    // KRYSTIAN FIXME: disambiguation based on signature
    for(auto& result : table.lookup(name))
    {
        if(for_nns)
        {
            // per [basic.lookup.qual.general] p1, when looking up a
            // component name of a nested-name-specifier, we only consider:
            // - namespaces,
            // - types, and
            // - templates whose specializations are types
            // KRYSTIAN FIXME: should we if the result is acceptable?
            if(result->isNamespace() ||
                result->isRecord() ||
                result->isEnum() ||
                result->isTypedef())
                return result;
        }
        else
        {
            // if we are looking up a terminal name, call the handler
            // to determine whether the result is acceptable
            if(callback(*result))
                return result;
        }
    }

    // if this is a record and nothing was found,
    // search base classes for the name
    if(context->isRecord())
    {
        const auto* RI = static_cast<
            const RecordInfo*>(context);
        // KRYSTIAN FIXME: resolve ambiguities & report errors
        for(const auto& B : RI->Bases)
        {
            if(const Info* result = lookupInContext(
                corpus_.find(B.Type->namedSymbol()),
                    name, for_nns, callback))
                return result;
        }
    }

    return nullptr;
}

const Info*
SymbolLookup::
lookupUnqualifiedImpl(
    const Info* context,
    std::string_view name,
    bool for_nns,
    LookupCallback& callback)
{
    if(! context)
        return nullptr;
    context = adjustLookupContext(context);
    while(context)
    {
        if(auto result = lookupInContext(
            context, name, for_nns, callback))
            return result;
        if(context->Namespace.empty())
            return nullptr;
        const SymbolID& parent = context->Namespace.front();
        context = corpus_.find(parent);
    }
    MRDOCS_UNREACHABLE();
}

const Info*
SymbolLookup::
lookupQualifiedImpl(
    const Info* context,
    std::span<const std::string_view> qualifier,
    std::string_view terminal,
    LookupCallback& callback)
{
    if(! context)
        return nullptr;
    if(qualifier.empty())
        return lookupInContext(
            context, terminal, false, callback);
    context = lookupUnqualifiedImpl(
        context, qualifier.front(), true, callback);
    qualifier = qualifier.subspan(1);
    if(! context)
        return nullptr;
    while(! qualifier.empty())
    {
        if(! (context = lookupInContext(
            context, qualifier.front(), true, callback)))
            return nullptr;
        qualifier = qualifier.subspan(1);
    }
    return lookupInContext(
        context, terminal, false, callback);
}

} // mrdocs
} // clang
