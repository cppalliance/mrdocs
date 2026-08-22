//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Metadata/Finalizers/AnchorFinalizer.hpp>
#include <mrdocs/Support/Radix.hpp>
#include <mrdocs/Support/Validate.hpp>
#include <mrdocs/ADT/UnorderedStringMap.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Support/TypeTraits/Concepts.hpp>
#include <mrdocs/Support/TypeTraits/TypeTraits.hpp>
#include <algorithm>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>

namespace mrdocs {
namespace {

std::string
getUnnamedInfoName(Symbol const& I)
{
    // All valid c++ identifiers begin with
    // an underscore or alphabetic character,
    // so a numeric prefix ensures no conflicts
    if (I.isFunction() || I.isOverloads())
    {
        static
        constexpr
        std::string_view
        func_reserved[] = {
            "2function",
            "2constructor",
            "2conversion",
            "2destructor"
        };
        std::size_t func_idx = 0;
        if (auto const* FI = I.asFunctionPtr())
        {
            // don't use the reserved prefix for overloaded operators
            if(FI->FuncClass == FunctionClass::Normal &&
                FI->OverloadedOperator != OperatorKind::None)
            {
                return std::string(getSafeOperatorName(
                    FI->OverloadedOperator, true));
            }
            func_idx = to_underlying(FI->FuncClass);
        }
        if (auto const* FI = I.asOverloadsPtr())
        {
            // don't use the reserved prefix for overloaded operators
            if(FI->FuncClass == FunctionClass::Normal &&
                FI->OverloadedOperator != OperatorKind::None)
            {
                return std::string(getSafeOperatorName(
                    FI->OverloadedOperator, true));
            }
            func_idx = to_underlying(FI->FuncClass);
        }
        MRDOCS_ASSERT(func_idx < std::size(func_reserved));
        return std::string(func_reserved[func_idx]);
    }

    std::size_t const idx = to_underlying(I.Kind) - 1;
    std::string res = "_";
    // push idx as two digits
    res.push_back(static_cast<char>('0' + (idx / 10)));
    res.push_back(static_cast<char>('0' + (idx % 10)));
    // push the name of the kind as kebab-case
    auto const kindStr = std::string(toString(I.Kind));
    res += toKebabCase(kindStr);
    return res;
}

// Computes each symbol's unqualified, URL-safe legible name, unique among
// its siblings (siblings that would collide are disambiguated with a
// suffix taken from the SymbolID). This is a build-time helper for the
// disambiguation, which is inherently a per-scope pass: a name's suffix
// length can grow as more same-named siblings are discovered, so the
// results are settled here before build() writes each unqualified name
// into the symbol's `Anchor`. The qualified fragment/path is assembled on
// demand by the generator from the parent chain, so it is not stored.
class LegibleNameTable
{
    Corpus const& corpus_;

    /* Name used for the global namespace

       This is typically "index" or "global"
       if a symbol has the same name as the
       global namespace, then it needs to be
       disambiguated
     */
    std::string global_ns_;

    /* The info related to the legible name for a symbol
     */
    struct LegibleName
    {
        /*  Raw unqualified name for the symbol
         */
        std::string unqualified;

        /*  Number of characters from the SymbolID string
            required to uniquely identify this symbol
         */
        std::uint8_t disambig_chars;

        /*  SymbolID converted to a string
         */
        std::string id_str;
    };

    /* A map from SymbolID to legible name information
     */
    std::unordered_map<SymbolID, LegibleName> map_;

    /* Maps unqualified names to all symbols with that name within the current
       scope. Keys match case-insensitively so that names differing only in
       case (which collide as a page or anchor on a case-insensitive
       filesystem) are disambiguated against each other.
     */
    UnorderedCIStringMultiMap<LegibleName*> disambiguation_map_;

public:
    /*  Build the map of legible names for all symbols in the corpus
     */
    LegibleNameTable(Corpus const& corpus, std::string_view const global_ns)
        : corpus_(corpus)
        , global_ns_(global_ns)
    {
        NamespaceSymbol const& global = corpus_.globalNamespace();

        // Treat the global namespace as-if its "name"
        // is in the same scope as its members
        buildLegibleMember(global, global_ns_);
        visit(global, *this);

        // after generating legible names for every symbol,
        // set the number of disambiguation characters
        // used for the global namespace to zero
        map_.at(global.id).disambig_chars = 0;
    }

    /*  Visit a symbol and build legible names for its members
     */
    template <class InfoTy>
    void
    operator()(InfoTy const& I)
    {
        if constexpr (SymbolParent<InfoTy> && !std::same_as<InfoTy, OverloadsSymbol>)
        {
            // Visit the members of the symbol and build legible names
            constexpr Corpus::TraverseOptions opts = {.skipInherited = true};
            corpus_.traverse(opts, I, [this, &I](Symbol const& M)
                {
                    auto const raw = getRawUnqualified(M);
                    buildLegibleMember(M, raw);

                    // Traverse non inherited function overloads inline
                    if (auto* MO = M.asOverloadsPtr())
                    {
                        corpus_.traverse(*MO, [this, &I](Symbol const& M2)
                            {
                                // Not inherited in regard to I
                                MRDOCS_CHECK_OR(M2.Parent == I.id);
                                auto const raw2 = getRawUnqualified(M2);
                                buildLegibleMember(M2, raw2);
                            });
                    }
                });

            // Clear the disambiguation map for this scope
            disambiguation_map_.clear();

            // Visit the members of the symbol to build legible names
            // for their members
            corpus_.traverse(opts, I, [this](Symbol const& M)
                {
                    visit(M, *this);
                });
        }
    }

    /*  Get the raw unqualified name for a symbol

        This function returns a reference to the original symbol
        name without any disambiguation characters.

        If the symbolhas no name, then a reserved name based
        on the type is returned instead.
     */
    std::string
    getRawUnqualified(SymbolID const& id)
    {
        Symbol const* I = corpus_.find(id);
        MRDOCS_ASSERT(I);
        return getRawUnqualified(*I);
    }

    /* @copydoc getRawUnqualified(SymbolID const&)
     */
    std::string
    getRawUnqualified(Symbol const& I)
    {
        MRDOCS_ASSERT(I.id && I.id != SymbolID::global);
        if (I.Name.empty())
        {
            return getUnnamedInfoName(I);
        }

        return visit(I, [&]<typename T>(T const& t) -> std::string
            {
                MRDOCS_ASSERT(!t.Name.empty());
                if constexpr(
                    T::isFunction() ||
                    T::isOverloads())
                {
                    // functions can be explicitly specialized,
                    // and can be overloaded
                    if (t.FuncClass != FunctionClass::Normal ||
                        t.OverloadedOperator != OperatorKind::None)
                    {
                        return getUnnamedInfoName(t);
                    }
                }
                else if constexpr(T::isUsing())
                {
                    if (t.Class == UsingClass::Normal && !t.ShadowDeclarations.empty())
                    {
                        return getRawUnqualified(t.ShadowDeclarations.front());
                    }
                }
                return t.Name;
            });
    }

    /* Take the raw unqualified name for a symbol and build a legible name
     */
    void
    buildLegibleMember(Symbol const& I,
        std::string_view rawName)
    {
        // Generate the legible name information for this symbol
        auto const idAsString = toBase16(std::string_view(
            reinterpret_cast<char const*>(I.id.data()), I.id.size()), true);
        LegibleName LI(std::string(rawName), 0, idAsString);
        LegibleName& info = map_.emplace(I.id, std::move(LI)).first->second;

        // Look for symbols with the same unqualified name. The map matches
        // case-insensitively, so a name that differs from a sibling only in
        // case is treated as a collision and disambiguated the same way an
        // exact duplicate is.
        auto [first, last] = disambiguation_map_.equal_range(rawName);
        auto sameNames = std::ranges::subrange(first, last) | std::views::values;
        if (std::ranges::empty(sameNames))
        {
            // Add this symbol to the disambiguation map
            disambiguation_map_.emplace(rawName, &info);
            // If there are no other symbols with the same name, then
            // disambiguation is not required.
            return;
        }

        // Iterate over the other symbols with the same raw unqualified name
        // and update their legible name information
        std::uint8_t suffix_size_required = 0;
        for (LegibleName* other : sameNames)
        {
            // Find the first character that differs between the two symbol IDs
            auto const mismatch_it = std::ranges::mismatch(
                info.id_str, other->id_str).in1;
            // Number of characters required to disambiguate
            std::uint8_t n_required = std::distance(info.id_str.begin(), mismatch_it) + 1;
            // Update the suffix size for the other symbol
            other->disambig_chars = std::max(n_required, other->disambig_chars);
            // Update the suffix size needed for this symbol
            suffix_size_required = std::max(suffix_size_required, n_required);
        }

        // Use the longest suffix needed to disambiguate
        // between all symbols with the same name in this scope
        info.disambig_chars = suffix_size_required;

        // Add this symbol to the disambiguation map
        disambiguation_map_.emplace(rawName, &info);
    }

    void
    getLegibleUnqualified(
        std::string& result,
        SymbolID const& id)
    {
        MRDOCS_ASSERT(corpus_.exists(id));

        // Find the legible name information for this symbol
        auto it = map_.find(id);
        if (it == map_.end())
        {
            // Late-build a legible name for this symbol.
            // NOTE: This may not perfectly disambiguate against siblings
            // because the per-scope disambiguation_map_ is ephemeral,
            // but it avoids a hard crash and produces a stable name.
            Symbol const& I = corpus_.get(id);
            auto const raw = getRawUnqualified(I);
            buildLegibleMember(I, raw);
            it = map_.find(I.id);
            if (it == map_.end())
            {
                // Final, robust fallback: emit the raw SymbolID (legible-ish),
                // and return without crashing.
                result.append(toBase16Str(I.id));
                return;
            }
        }

        auto& [unqualified, n_disambig, id_str] = it->second;

        // Append the unqualified name to the result
        // The unqualified name has no reserved chars
        result.reserve(
            result.size() +
            unqualified.size() +
            n_disambig ? n_disambig + 2 : 0);
        result.append(unqualified);

        // Append a disambiguation suffix from the symbol ID if needed
        if (n_disambig)
        {
            // KRYSTIAN FIXME: the SymbolID chars must be prefixed with
            // a reserved character, otherwise there could be a
            // conflict with a name in an inner scope. this could be
            // resolved by using the base-10 representation of the SymbolID
            result.append("-0");
            result.append(id_str.c_str(), n_disambig);
        }
    }

    /* Return the disambiguated unqualified legible name for a symbol.

       The name is unique among the symbol's siblings. The qualified form
       is assembled on demand by the generator, which walks the parent
       chain joining these unqualified anchors, so it is not stored here.
     */
    std::string
    getUnqualified(SymbolID const& id)
    {
        std::string result;
        getLegibleUnqualified(result, id);
        return result;
    }
};

} // (anon)

void
AnchorFinalizer::
build()
{
    // The anchor is the symbol's own unqualified name only: its hashed id
    // when legible names are disabled, otherwise its unique, URL-safe
    // legible name (unique among its siblings). The generator assembles
    // the qualified fragment or multipage path on demand by walking the
    // parent chain and joining these per-symbol anchors.
    if (!config_.legibleNames)
    {
        for (Symbol const& I : corpus_)
        {
            if (Symbol* M = corpus_.find(I.id))
            {
                M->Anchor = toBase16Str(I.id);
            }
        }
        return;
    }

    LegibleNameTable names(corpus_, "index");
    for (Symbol const& I : corpus_)
    {
        if (Symbol* M = corpus_.find(I.id))
        {
            M->Anchor = names.getUnqualified(I.id);
        }
    }
}

} // mrdocs
