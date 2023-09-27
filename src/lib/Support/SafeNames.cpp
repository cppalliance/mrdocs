//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "lib/Support/Radix.hpp"
#include "lib/Support/SafeNames.hpp"
#include "lib/Support/Validate.hpp"
#include "lib/Support/Debug.hpp"
#include <mrdox/Corpus.hpp>
#include <mrdox/Metadata.hpp>
#include <mrdox/Platform.hpp>
#include <mrdox/Support/TypeTraits.hpp>
#include <fmt/format.h>
#include <algorithm>
#include <ranges>
#include <string_view>
#include <unordered_map>

namespace clang {
namespace mrdox {

class SafeNames::Impl
{
    Corpus const& corpus_;

    // store all info required to generate a safename
    struct SafeNameInfo
    {
        // safename without disambiguation characters
        std::string_view unqualified;
        // number of characters from the SymbolID string
        // required to uniquely identify this symbol
        std::uint8_t disambig_chars;
        // SymbolID converted to a string
        std::string id_str;
    };

    std::unordered_map<SymbolID, SafeNameInfo> map_;

    std::string_view
    getReserved(const Info& I)
    {
        // all valid c++ identifiers begin with
        // an underscore or alphabetic character,
        // so a numeric prefix ensures no conflicts
        static
        std::string_view
        reserved[] = {
            "0namespace",
            "1record",
            "2function",
            "3enum",
            "4typedef",
            "5variable",
            "6field",
            "7specialization",
        };
        if(I.isFunction())
        {
            static
            std::string_view
            func_reserved[] = {
                "2function",
                "2constructor",
                "2conversion",
                "2destructor",
                "2deduction_guide",
            };
            const auto& FI = static_cast<
                const FunctionInfo&>(I);
            // don't use the reserved prefix for overloaded operators
            if(FI.Class == FunctionClass::Normal &&
                FI.specs0.overloadedOperator.get() !=
                    OperatorKind::None)
            {
                return getSafeOperatorName(
                    FI.specs0.overloadedOperator.get(), true);
            }
            std::size_t func_idx = to_underlying(FI.Class);
            MRDOX_ASSERT(func_idx < std::size(func_reserved));
            return func_reserved[func_idx];
        }

        std::size_t idx = to_underlying(I.Kind);
        MRDOX_ASSERT(idx < std::size(reserved));
        return reserved[idx];
    }

    std::string_view
    getUnqualified(
        const Info& I)
    {
        return visit(I, [&]<typename T>(
            const T& t) -> std::string_view
            {
                // namespaces can be unnamed (i.e. anonymous)
                if constexpr(T::isNamespace())
                {
                    // special case for the global namespace
                    if(t.id == SymbolID::zero)
                        return std::string_view();

                    if(t.specs.isAnonymous.get())
                        return getReserved(t);
                    MRDOX_ASSERT(! t.Name.empty());
                    return t.Name;
                }
                // fields and typedefs cannot be overloaded
                // or partially/explicitly specialized,
                // but must have names
                if constexpr(
                    T::isField() ||
                    T::isTypedef())
                {
                    MRDOX_ASSERT(! t.Name.empty());
                    return t.Name;
                }

                // variables can be partially/explicitly
                // specialized, but must have names and
                // cannot be overloaded
                if constexpr(T::isVariable())
                {
                    MRDOX_ASSERT(! t.Name.empty());
                    return t.Name;
                }

                // enums cannot be overloaded or partially/
                // explicitly specialized, but can be unnamed
                if constexpr(T::isEnum())
                {
                    /** KRYSTIAN FIXME: [dcl.enum] p12 states (paraphrased):

                        an unnamed enumeration type that has a first enumerator
                        and does not have a typedef name for linkage purposes
                        is denoted by its underlying type and its
                        first enumerator for linkage purposes.

                        should we also take this approach? note that this would not
                        address unnamed enumeration types without any enumerators.
                    */
                    if(t.Name.empty())
                        return getReserved(t);
                    return t.Name;
                }

                // records can be partially/explicitly specialized,
                // and can be unnamed, but cannot be overloaded
                if constexpr(T::isRecord())
                {
                    if(t.Name.empty())
                        return getReserved(t);
                    return t.Name;
                }

                // functions must have named,
                // can be explicitly specialized,
                // and can be overloaded
                if constexpr(T::isFunction())
                {
                    if(t.Class != FunctionClass::Normal ||
                        t.specs0.overloadedOperator.get() != OperatorKind::None)
                        return getReserved(t);
                    MRDOX_ASSERT(! t.Name.empty());
                    return t.Name;
                }

                if constexpr(T::isSpecialization())
                {
                    MRDOX_ASSERT(! t.Name.empty());
                    return t.Name;
                }

                MRDOX_UNREACHABLE();
            });
    }

    //--------------------------------------------

    template<typename Range>
    void
    buildSafeMembers(
        const Range& Members)
    {
        // maps unqualified names to a vector of all symbols
        // with that name within the current scope
        std::unordered_map<std::string_view,
            std::vector<SymbolID>> disambiguation_map;
        disambiguation_map.reserve(Members.size());

        for(const SymbolID& id : Members)
        {
            const Info* I = corpus_.find(id);
            if(! I)
                continue;
            // generate the unqualified name and SymbolID string
            auto [it, emplaced] = map_.emplace(I->id, SafeNameInfo(
                getUnqualified(*I), 0, toBase16(I->id, true)));
            // update the disambiguation map
            auto [disambig_it, disambig_emplaced] =
                disambiguation_map.try_emplace(it->second.unqualified);

            // if there are other symbols with the same name, then disambiguation
            // is required. iterate over the other symbols with the same unqualified name,
            // and calculate the minimum number of characters from the SymbolID needed
            // to uniquely identify each symbol. then, update all symbols with the new value.
            if(! disambig_emplaced)
            {
                std::uint8_t n_chars = 0;
                std::string_view id_str = it->second.id_str;
                for(const SymbolID& other_id : disambig_it->second)
                {
                    auto& other = map_.find(other_id)->second;
                    auto [mismatch_id, mismatch_other] =
                        std::mismatch(id_str.begin(), id_str.end(),
                            other.id_str.begin(), other.id_str.end());
                    std::uint8_t n_required = std::distance(
                        id_str.begin(), mismatch_id) + 1;
                    n_chars = std::max({
                        n_chars, other.disambig_chars, n_required});
                }

                MRDOX_ASSERT(n_chars);
                // update the number of disambiguation characters for each symbol
                it->second.disambig_chars = n_chars;
                for(const SymbolID& other_id : disambig_it->second)
                    map_.find(other_id)->second.disambig_chars = n_chars;

            }
            disambig_it->second.push_back(I->id);
        }
    }

    //--------------------------------------------

public:
    explicit
    Impl(Corpus const& corpus)
        : corpus_(corpus)
    {
        map_.try_emplace(SymbolID::zero, SafeNameInfo(
            "global_namespace", 0, toBase16(SymbolID::zero, true)));

        visit(corpus_.globalNamespace(), *this);
    }

    void
    getSafeUnqualified(
        std::string& result,
        const SymbolID& id)
    {
        MRDOX_ASSERT(corpus_.exists(id));
        auto const it = map_.find(id);
        MRDOX_ASSERT(it != map_.end());
        auto& [unqualified, n_disambig, id_str] = it->second;
        result.reserve(
            result.size() +
            unqualified.size() +
            n_disambig ? n_disambig + 2 : 0);
        result.append(unqualified);
        if(n_disambig)
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
    getSafeQualified(
        std::string& result,
        const SymbolID& id,
        char delim)
    {
        MRDOX_ASSERT(corpus_.exists(id));
        auto const& parents = corpus_.get(id).Namespace;
        if(parents.size() > 1)
        {
            for(auto const& parent : parents |
                std::views::reverse |
                std::views::drop(1))
            {
                getSafeUnqualified(result, parent);
                result.push_back(delim);
            }
        }
        getSafeUnqualified(result, id);
    }

    template<class T>
    void operator()(T const& I)
    {
        if constexpr(T::isNamespace() || T::isRecord())
        {
            buildSafeMembers(I.Members);
            for(const SymbolID& C : I.Members)
            {
                if(const Info* CI = corpus_.find(C))
                {
                    visit(*CI, *this);
                }
            }
        }

        if constexpr(T::isSpecialization())
        {
            buildSafeMembers(
                std::views::transform(I.Members,
                    [](const SpecializedMember& M) ->
                        const SymbolID&
                    {
                        return M.Specialized;
                    }));
            for(const SpecializedMember& M : I.Members)
            {
                if(const Info* CI = corpus_.find(M.Specialized))
                {
                    visit(*CI, *this);
                }
            }
        }
    }
};

//------------------------------------------------

SafeNames::
SafeNames(Corpus const& corpus)
    : impl_(std::make_unique<Impl>(corpus))
{
}

SafeNames::
~SafeNames() noexcept = default;

std::string
SafeNames::
getUnqualified(
    SymbolID const& id) const
{
    std::string result;
    impl_->getSafeUnqualified(result, id);
    return result;
}

std::string
SafeNames::
getQualified(
    SymbolID const& id,
    char delim) const
{
    std::string result;
    impl_->getSafeQualified(result, id, delim);
    return result;
}

} // mrdox
} // clang
