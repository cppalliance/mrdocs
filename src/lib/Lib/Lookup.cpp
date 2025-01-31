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

namespace clang::mrdocs {

namespace {

bool
supportsLookup(Info const* I)
{
    MRDOCS_CHECK_OR(I, false);
    return visit(*I, []<typename InfoTy>(
        InfoTy const&) -> bool
    {
        return InfoParent<InfoTy>;
    });
}

bool isTransparent(Info const* info)
{
    MRDOCS_ASSERT(info);
    return visit(*info, []<typename InfoTy>(
        InfoTy const& I) -> bool
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
    Corpus const& corpus,
    Info const& info,
    LookupTable& lookups)
{
    visit(info, [&]<typename InfoTy>(InfoTy const& I)
    {
        if constexpr(InfoParent<InfoTy>)
        {
            for (SymbolID const& M : allMembers(I))
            {
                Info const* child = corpus.find(M);
                // if the member is an inline namespace or
                // an unscoped enumeration, add its members as well
                if (isTransparent(child))
                {
                    buildLookups(corpus, *child, lookups);
                }

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
    Info const& info,
    Corpus const& corpus)
{
    MRDOCS_ASSERT(supportsLookup(&info));
    buildLookups(corpus, info, *this);
}

SymbolLookup::
SymbolLookup(Corpus const& corpus)
    : corpus_(corpus)
{
    for(Info const& I : corpus_)
    {
        if (!supportsLookup(&I))
        {
            continue;
        }
        lookup_tables_.emplace(&I, LookupTable(I, corpus_));
    }
}

Info const*
SymbolLookup::
adjustLookupContext(
    Info const* context)
{
    // find the innermost enclosing context that supports name lookup
    while(! supportsLookup(context))
    {
        MRDOCS_ASSERT(context->Parent);
        context = corpus_.find(context->Parent);
    }
    return context;
}

Info const*
SymbolLookup::
lookThroughTypedefs(Info const* I)
{
    if (!I || !I->isTypedef())
    {
        return I;
    }
    auto* TI = static_cast<TypedefInfo const*>(I);
    return lookThroughTypedefs(
        corpus_.find(TI->Type->namedSymbol()));
}

Info const*
SymbolLookup::
lookupInContext(
    Info const* context,
    std::string_view const name,
    bool const for_nns,
    LookupCallback& callback)
{
    // if the lookup context is a typedef, we want to
    // lookup the name in the type it denotes
    if (!((context = lookThroughTypedefs(context))))
    {
        return nullptr;
    }
    MRDOCS_ASSERT(supportsLookup(context));
    // KRYSTIAN FIXME: disambiguation based on signature
    auto const it = lookup_tables_.find(context);
    if (it == lookup_tables_.end())
    {
        return nullptr;
    }
    for (LookupTable const& table = it->second;
         auto& result : table.lookup(name))
    {
        if(for_nns)
        {
            // per [basic.lookup.qual.general] p1, when looking up a
            // component name of a nested-name-specifier, we only consider:
            // - namespaces,
            // - types, and
            // - templates whose specializations are types
            // KRYSTIAN FIXME: should we if the result is acceptable?
            if (result->isNamespace() ||
                result->isRecord() ||
                result->isEnum() ||
                result->isTypedef())
                return result;
        }
        else
        {
            // if we are looking up a terminal name, call the handler
            // to determine whether the result is acceptable
            if (callback(*result))
            {
                return result;
            }
        }
    }

    // if this is a record and nothing was found,
    // search base classes for the name
    if(context->isRecord())
    {
        auto const* RI = static_cast<RecordInfo const*>(context);
        // KRYSTIAN FIXME: resolve ambiguities & report errors
        for(auto const& B : RI->Bases)
        {
            if (Info const* result = lookupInContext(
                    corpus_.find(B.Type->namedSymbol()),
                    name,
                    for_nns,
                    callback))
            {
                return result;
            }
        }
    }

    return nullptr;
}

Info const*
SymbolLookup::
lookupUnqualifiedImpl(
    Info const* context,
    std::string_view const name,
    bool const for_nns,
    LookupCallback& callback)
{
    if (!context)
    {
        return nullptr;
    }
    context = adjustLookupContext(context);
    while(context)
    {
        if (auto result = lookupInContext(context, name, for_nns, callback))
        {
            return result;
        }
        if (!context->Parent)
        {
            return nullptr;
        }
        context = corpus_.find(context->Parent);
    }
    MRDOCS_UNREACHABLE();
}

Info const*
SymbolLookup::
lookupQualifiedImpl(
    Info const* context,
    std::span<const std::string_view> qualifier,
    std::string_view const terminal,
    LookupCallback& callback)
{
    if (!context)
    {
        return nullptr;
    }
    if (qualifier.empty())
    {
        return lookupInContext(context, terminal, false, callback);
    }
    context = lookupUnqualifiedImpl(
        context, qualifier.front(), true, callback);
    qualifier = qualifier.subspan(1);
    if (!context)
    {
        return nullptr;
    }
    while(! qualifier.empty())
    {
        if (!((context
               = lookupInContext(context, qualifier.front(), true, callback))))
        {
            return nullptr;
        }
        qualifier = qualifier.subspan(1);
    }
    return lookupInContext(
        context, terminal, false, callback);
}

} // clang::mrdocs
