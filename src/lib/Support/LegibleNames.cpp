//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "lib/Support/Radix.hpp"
#include "lib/Support/LegibleNames.hpp"
#include "lib/Support/Validate.hpp"
#include "lib/Support/Debug.hpp"
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Support/Concepts.hpp>
#include <mrdocs/Platform.hpp>
#include <mrdocs/Support/TypeTraits.hpp>
#include <fmt/format.h>
#include <algorithm>
#include <ranges>
#include <string_view>
#include <unordered_map>

namespace clang::mrdocs {
namespace {

std::string_view
getUnnamedInfoName(Info const& I)
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
        if (auto const* FI = dynamic_cast<FunctionInfo const*>(&I))
        {
            // don't use the reserved prefix for overloaded operators
            if(FI->Class == FunctionClass::Normal &&
                FI->OverloadedOperator != OperatorKind::None)
            {
                return getSafeOperatorName(
                    FI->OverloadedOperator, true);
            }
            func_idx = to_underlying(FI->Class);
        }
        if (auto const* FI = dynamic_cast<OverloadsInfo const*>(&I))
        {
            // don't use the reserved prefix for overloaded operators
            if(FI->Class == FunctionClass::Normal &&
                FI->OverloadedOperator != OperatorKind::None)
            {
                return getSafeOperatorName(
                    FI->OverloadedOperator, true);
            }
            func_idx = to_underlying(FI->Class);
        }
        MRDOCS_ASSERT(func_idx < std::size(func_reserved));
        return func_reserved[func_idx];
    }

    static
    constexpr
    std::string_view
    reserved[] = {
        "00namespace",
        "01record",
        "02function",
        "03overloads",
        "04enum",
        "05enum-constant",
        "06typedef",
        "07variable",
        "08field",
        "09friend",
        "10guide",
        "11namespace-alias",
        "12using",
        "13concept"
    };
    std::size_t const idx = to_underlying(I.Kind) - 1;
    MRDOCS_ASSERT(idx < std::size(reserved));
    return reserved[idx];
}
} // namespace

class LegibleNames::Impl
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
    struct LegibleNameInfo
    {
        /*  Raw unqualified name for the symbol
         */
        std::string_view unqualified;

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
    std::unordered_map<SymbolID, LegibleNameInfo> map_;

    /* Maps unqualified names to all symbols with that name within the current scope
     */
    std::unordered_multimap<std::string_view, LegibleNameInfo*> disambiguation_map_;

public:
    /*  Build the map of legible names for all symbols in the corpus
     */
    Impl(Corpus const& corpus, std::string_view const global_ns)
        : corpus_(corpus)
        , global_ns_(global_ns)
    {
        NamespaceInfo const& global = corpus_.globalNamespace();

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
        if constexpr (InfoParent<InfoTy> && !std::same_as<InfoTy, OverloadsInfo>)
        {
            // Visit the members of the symbol and build legible names
            constexpr Corpus::TraverseOptions opts = {.skipInherited = true};
            corpus_.traverse(opts, I, [this, &I](Info const& M)
                {
                    auto const raw = getRawUnqualified(M);
                    buildLegibleMember(M, raw);

                    // Traverse non inherited function overloads inline
                    if (M.isOverloads())
                    {
                        auto const& O = static_cast<OverloadsInfo const&>(M);
                        corpus_.traverse(O, [this, &I](Info const& M2)
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
            corpus_.traverse(opts, I, [this](Info const& M)
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
    std::string_view
    getRawUnqualified(SymbolID const& id)
    {
        Info const* I = corpus_.find(id);
        MRDOCS_ASSERT(I);
        return getRawUnqualified(*I);
    }

    /* @copydoc getRawUnqualified(SymbolID const&)
     */
    std::string_view
    getRawUnqualified(Info const& I)
    {
        MRDOCS_ASSERT(I.id && I.id != SymbolID::global);
        if (I.Name.empty())
        {
            return getUnnamedInfoName(I);
        }

        return visit(I, [&]<typename T>(T const& t) -> std::string_view
            {
                MRDOCS_ASSERT(!t.Name.empty());
                if constexpr(
                    T::isFunction() ||
                    T::isOverloads())
                {
                    // functions can be explicitly specialized,
                    // and can be overloaded
                    if (t.Class != FunctionClass::Normal ||
                        t.OverloadedOperator != OperatorKind::None)
                    {
                        return getUnnamedInfoName(t);
                    }
                }
                else if constexpr(T::isFriend())
                {
                    return getUnnamedInfoName(t);
                }
                return t.Name;
            });
    }

    /* Take the raw unqualified name for a symbol and build a legible name
     */
    void
    buildLegibleMember(
        Info const& I,
        std::string_view rawName)
    {
        // Generate the legible name information for this symbol
        auto const idAsString = toBase16(I.id, true);
        LegibleNameInfo LI(rawName, 0, idAsString);
        LegibleNameInfo& info = map_.emplace(I.id, LI).first->second;

        // Look for symbols with the same unqualified name
        auto [first, last] = disambiguation_map_.equal_range(rawName);
        auto sameNameInfos = std::ranges::subrange(first, last) | std::views::values;
        if (std::ranges::empty(sameNameInfos))
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
        for (LegibleNameInfo* other : sameNameInfos)
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
        auto const it = map_.find(id);
        MRDOCS_ASSERT(it != map_.end());
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

    void
    getLegibleQualified(
        std::string& result,
        SymbolID const& id,
        char const delim)
    {
        MRDOCS_ASSERT(corpus_.exists(id));
        auto const& I = corpus_.get(id);
        auto const curParent = I.Parent;
        if (curParent != SymbolID::invalid &&
            curParent != SymbolID::global)
        {
            getLegibleQualified(result, curParent, delim);
            result.push_back(delim);
        }
        getLegibleUnqualified(result, id);
    }
};

//------------------------------------------------

LegibleNames::
LegibleNames(
    Corpus const& corpus,
    bool const enabled)
{
    if (enabled)
    {
        impl_ = std::make_unique<Impl>(corpus, "index");
    }
}

LegibleNames::
~LegibleNames() noexcept = default;

std::string
LegibleNames::
getUnqualified(
    SymbolID const& id) const
{
    if (!impl_)
    {
        return toBase16(id);
    }
    std::string result;
    impl_->getLegibleUnqualified(result, id);
    return result;
}

std::string
LegibleNames::
getQualified(
    SymbolID const& id,
    char const delim) const
{
    if (!impl_)
    {
        return toBase16(id);
    }
    std::string result;
    impl_->getLegibleQualified(result, id, delim);
    return result;
}

} // clang::mrdocs
