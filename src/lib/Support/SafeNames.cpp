//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "lib/Support/Radix.hpp"
#include "lib/Support/SafeNames.hpp"
#include "lib/Support/Validate.hpp"
#include "lib/Support/Debug.hpp"
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Platform.hpp>
#include <mrdocs/Support/TypeTraits.hpp>
#include <fmt/format.h>
#include <algorithm>
#include <ranges>
#include <string_view>
#include <unordered_map>

namespace clang {
namespace mrdocs {

class SafeNames::Impl
{
    Corpus const& corpus_;

    // name used for the global namespace
    std::string global_ns_;

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

    // maps unqualified names to all symbols
    // with that name within the current scope
    std::unordered_multimap<std::string_view,
        SafeNameInfo*> disambiguation_map_;

    std::string_view
    getReserved(const Info& I)
    {
        // all valid c++ identifiers begin with
        // an underscore or alphabetic character,
        // so a numeric prefix ensures no conflicts
        static
        std::string_view
        reserved[] = {
            "00namespace",
            "01record",
            "02function",
            "03enum",
            "04typedef",
            "05variable",
            "06field",
            "07specialization",
            "08friend",
            "09enumeration",
            "10guide",
            "11alias",
            "12using",
        };
        if(I.isFunction())
        {
            static
            std::string_view
            func_reserved[] = {
                "2function",
                "2constructor",
                "2conversion",
                "2destructor"
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
            MRDOCS_ASSERT(func_idx < std::size(func_reserved));
            return func_reserved[func_idx];
        }

        std::size_t idx = to_underlying(I.Kind) - 1;
        MRDOCS_ASSERT(idx < std::size(reserved));
        return reserved[idx];
    }

public:
    std::string_view
    getUnqualified(
        const SymbolID& id)
    {
        const Info* I = corpus_.find(id);
        MRDOCS_ASSERT(I);
        return getUnqualified(*I);
    }

    std::string_view
    getUnqualified(
        const Info& I)
    {
        MRDOCS_ASSERT(I.id && I.id != SymbolID::global);
        return visit(I, [&]<typename T>(
            const T& t) -> std::string_view
            {
                // namespaces can be unnamed (i.e. anonymous)
                if constexpr(T::isNamespace())
                {
                    if(t.specs.isAnonymous.get())
                        return getReserved(t);
                    MRDOCS_ASSERT(! t.Name.empty());
                    return t.Name;
                }
                // fields and typedefs cannot be overloaded
                // or partially/explicitly specialized,
                // but must have names
                if constexpr(
                    T::isField() ||
                    T::isTypedef())
                {
                    MRDOCS_ASSERT(! t.Name.empty());
                    return t.Name;
                }

                // variables can be partially/explicitly
                // specialized, but must have names and
                // cannot be overloaded
                if constexpr(T::isVariable())
                {
                    MRDOCS_ASSERT(! t.Name.empty());
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
                    MRDOCS_ASSERT(! t.Name.empty());
                    return t.Name;
                }

                if constexpr(T::isSpecialization())
                {
                    MRDOCS_ASSERT(! t.Name.empty());
                    return t.Name;
                }

                if constexpr(T::isFriend())
                {
                    return getReserved(t);
                }

                if constexpr(T::isAlias())
                {
                    MRDOCS_ASSERT(! t.Name.empty());
                    return t.Name;
                }

                if constexpr(T::isUsing())
                {
                    MRDOCS_ASSERT(! t.Name.empty());
                    return t.Name;
                }

                if constexpr(T::isEnumerator())
                {
                    MRDOCS_ASSERT(! t.Name.empty());
                    return t.Name;
                }

                if constexpr(T::isGuide())
                {
                    MRDOCS_ASSERT(! t.Name.empty());
                    return t.Name;
                }

                MRDOCS_UNREACHABLE();
            });
    }

    //--------------------------------------------

    #define MINIMAL_SUFFIX

    void
    buildSafeMember(
        const Info& I,
        std::string_view name)
    {
        // generate the unqualified name and SymbolID string
        SafeNameInfo& info = map_.emplace(I.id, SafeNameInfo(
            name, 0, toBase16(I.id, true))).first->second;
        // if there are other symbols with the same name, then disambiguation
        // is required. iterate over the other symbols with the same unqualified name,
        // and calculate the minimum number of characters from the SymbolID needed
        // to uniquely identify each symbol. then, update all symbols with the new value.
        std::uint8_t max_required = 0;
        auto [first, last] = disambiguation_map_.equal_range(name);
        for(const auto& other : std::ranges::subrange(
            first, last) | std::views::values)
        {
            auto mismatch_it = std::ranges::mismatch(
                info.id_str, other->id_str).in1;
            std::uint8_t n_required = std::distance(
                info.id_str.begin(), mismatch_it) + 1;
            #ifndef MINIMAL_SUFFIX
                max_required = std::max({max_required,
                    other->disambig_chars, n_required});
            #else
                // update the suffix size for the other symbol
                other->disambig_chars = std::max(
                    n_required, other->disambig_chars);
                // update the suffix size needed for this symbol
                max_required = std::max(max_required, n_required);
            #endif
        }

        #ifndef MINIMAL_SUFFIX
            if(max_required)
            {
                // update the number of disambiguation characters for each symbol
                for(const auto& other : std::ranges::subrange(
                    first, last) | std::views::values)
                    other->disambig_chars = max_required;
                info.disambig_chars = max_required;
            }
        #else
            // use the longest suffix needed to disambiguate
            // between all symbols with the same name in this scope
            info.disambig_chars = max_required;
        #endif
        // add this symbol to the disambiguation map
        disambiguation_map_.emplace(name, &info);
    }

    //--------------------------------------------

    template<typename InfoTy, typename Fn>
    void traverse(const InfoTy& I, Fn&& F)
    {
        if constexpr(
            InfoTy::isSpecialization() ||
            InfoTy::isNamespace() ||
            InfoTy::isRecord() ||
            InfoTy::isEnum())
        {
            for(const SymbolID& id : I.Members)
                F(id);
        }
    }

    //--------------------------------------------

public:
    Impl(Corpus const& corpus, std::string_view global_ns)
        : corpus_(corpus)
        , global_ns_(global_ns)
    {
        const NamespaceInfo& global =
            corpus_.globalNamespace();
        // treat the global namespace as-if its "name"
        // is in the same scope as its members
        buildSafeMember(global, global_ns_);
        visit(global, *this);
        // after generating safenames for every symbol,
        // set the number of disambiguation characters
        // used for the global namespace to zero
        map_.at(global.id).disambig_chars = 0;
    }

    template<typename InfoTy>
    void operator()(const InfoTy& I)
    {
        traverse(I, [this](const SymbolID& id)
            {
                if(const Info* M = corpus_.find(id))
                    buildSafeMember(*M, getUnqualified(*M));
            });
        // clear the disambiguation map after visiting the members,
        // then build disambiguation information for each member
        disambiguation_map_.clear();
        traverse(I, [this](const SymbolID& id)
            {
                if(const Info* M = corpus_.find(id))
                    visit(*M, *this);
            });
    }

    void
    getSafeUnqualified(
        std::string& result,
        const SymbolID& id)
    {
        MRDOCS_ASSERT(corpus_.exists(id));
        auto const it = map_.find(id);
        MRDOCS_ASSERT(it != map_.end());
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
        MRDOCS_ASSERT(corpus_.exists(id));
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
};

//------------------------------------------------

SafeNames::
SafeNames(
    Corpus const& corpus,
    bool enabled)
{
    if(enabled)
        impl_ = std::make_unique<Impl>(corpus, "index");
}

SafeNames::
~SafeNames() noexcept = default;

std::string
SafeNames::
getUnqualified(
    SymbolID const& id) const
{
    if(! impl_)
        return toBase16(id);
    std::string result;
    impl_->getSafeUnqualified(result, id);
    return result;
}

std::string
SafeNames::
getUnqualified(
    OverloadSet const& os) const
{
    std::string result = "overload-";
    // KRYSTIAN FIXME: the name needs to be hashed
    result.append(os.Name);
    return result;
}

std::string
SafeNames::
getQualified(
    SymbolID const& id,
    char delim) const
{
    if(! impl_)
        return toBase16(id);
    std::string result;
    impl_->getSafeQualified(result, id, delim);
    return result;
}

std::string
SafeNames::
getQualified(
    OverloadSet const& os,
    char delim) const
{
    if(! impl_)
        return getUnqualified(os);
    std::string result;
    if(os.Parent != SymbolID::global)
    {
        impl_->getSafeQualified(result, os.Parent, delim);
        result.push_back(delim);
    }
    // the safename for an overload set is the unqualified
    // safe name of its members, without any disambiguation characters.
    // members of an overload set use the same safe name regardless of
    // whether they belong to an overload set
    result.append(impl_->getUnqualified(
        os.Members.front()));
    return result;
}

} // mrdocs
} // clang
