//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "lib/Lib/ConfigImpl.hpp"
#include "lib/Support/Debug.hpp"
#include <mrdocs/Metadata/Interface.hpp>
#include <mrdocs/Support/TypeTraits.hpp>
#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Metadata/Enum.hpp>
#include <mrdocs/Metadata/Function.hpp>
#include <mrdocs/Metadata/Overloads.hpp>
#include <mrdocs/Metadata/Record.hpp>
#include <mrdocs/Metadata/Typedef.hpp>
#include <mrdocs/Metadata/Variable.hpp>
#include <algorithm>
#include <ranges>
#include <variant>

namespace clang {
namespace mrdocs {

//------------------------------------------------

class Interface::Build
{
    Interface& I_;
    Corpus const& corpus_;
    bool includePrivate_;

    template<class T>
    using Table = std::vector<
        std::pair<AccessKind, T const*>>;

    Table<RecordInfo>       records_;
    Table<FunctionInfo>     functions_;
    Table<EnumInfo>         enums_;
    Table<TypedefInfo>      types_;
    Table<FieldInfo>        data_;
    Table<FunctionInfo>     staticfuncs_;
    Table<VariableInfo>     staticdata_;
    Table<FriendInfo>       friends_;

    void addOverload(const FunctionInfo& F, ScopeInfo& S)
    {
        if(std::ranges::find(S.Members, F.id) == S.Members.end())
            S.Members.emplace_back(F.id);
        auto& lookups = S.Lookups.try_emplace(F.Name).first->second;
        if(std::ranges::find(lookups, F.id) == lookups.end())
            lookups.emplace_back(F.id);
    }

    const Info*
    getTypeAsTag(
        const std::unique_ptr<TypeInfo>& T)
    {
        if(! T)
            return nullptr;
        SymbolID id = visit(*T, []<typename T>(const T& t)
        {
            if constexpr(T::isTag() || T::isSpecialization())
                return t.id;
            return SymbolID::invalid;
        });
        if(! id)
            return nullptr;
        return corpus_.find(id);
    }


    const Info*
    lookThroughTypedefs(const Info* I)
    {
        if(! I || ! I->isTypedef())
            return I;
        auto* TI = static_cast<const TypedefInfo*>(I);
        return lookThroughTypedefs(getTypeAsTag(TI->Type));
    }

public:
    Build(
        Interface& I,
        RecordInfo const& Derived,
        Corpus const& corpus) noexcept
        : I_(I)
        , corpus_(corpus)
    {
        auto& config = static_cast<
            ConfigImpl const&>(corpus_.config);
        includePrivate_ = config->inaccessibleMembers !=
            ConfigImpl::SettingsImpl::ExtractPolicy::Never;
        // treat `Derived` as a public base,
        append(AccessKind::Public, Derived, true);

        finish();
    }

private:
    static
    AccessKind
    effectiveAccess(
        AccessKind memberAccess,
        AccessKind baseAccess) noexcept
    {
        if(memberAccess == AccessKind::Private ||
            baseAccess == AccessKind::Private)
            return AccessKind::Private;
        if(memberAccess == AccessKind::Protected ||
            baseAccess == AccessKind::Protected)
            return AccessKind::Protected;
        return AccessKind::Public;
    }

    void
    append(
        AccessKind access,
        RecordInfo const& From,
        bool isDerived)
    {
        for(auto const& B : From.Bases)
        {
            auto actualAccess = effectiveAccess(access, B.Access);

            if( ! includePrivate_ &&
                actualAccess == AccessKind::Private)
                continue;
            const Info* Base = lookThroughTypedefs(
                getTypeAsTag(B.Type));
            if(! Base || ! Base->isRecord())
                continue;

            append(actualAccess, *static_cast<
                const RecordInfo*>(Base), false);
        }
        for(auto const& id : From.Members)
        {
            const auto& I = corpus_.get<Info>(id);
            auto actualAccess = effectiveAccess(access, I.Access);
            if(I.Kind == InfoKind::Function)
            {
                const auto& F = static_cast<FunctionInfo const&>(I);
                // do not inherit constructors, destructors,
                // or deduction guides
                if(!isDerived &&
                    (F.Class == FunctionClass::Constructor ||
                    F.Class == FunctionClass::Destructor ||
                    F.Class == FunctionClass::Deduction))
                    continue;
                auto const isRecFinal = From.specs.isFinal.get();
                if( includePrivate_ ||
                    actualAccess != AccessKind::Private || (
                        ! isRecFinal && F.specs0.isVirtual))
                {
                    if(F.specs0.storageClass == StorageClassKind::Static)
                    {
                        addOverload(F, I_.StaticOverloads);
                        staticfuncs_.push_back({ actualAccess, &F });
                    }
                    else
                    {
                        addOverload(F, I_.Overloads);
                        functions_.push_back({ actualAccess, &F });
                    }
                }
                continue;
            }
            else if(! includePrivate_ &&
                actualAccess == AccessKind::Private)
            {
                continue;
            }
            switch(I.Kind)
            {
            case InfoKind::Enum:
                enums_.push_back({ actualAccess,
                    static_cast<const EnumInfo*>(&I) });
                break;
            case InfoKind::Field:
                data_.push_back({ actualAccess,
                    static_cast<const FieldInfo*>(&I) });
                break;
            case InfoKind::Record:
                records_.push_back({ actualAccess,
                    static_cast<const RecordInfo*>(&I) });
                break;
            case InfoKind::Typedef:
                types_.push_back({ actualAccess,
                    static_cast<const TypedefInfo*>(&I) });
                break;
            case InfoKind::Variable:
                staticdata_.push_back({ actualAccess,
                    static_cast<const VariableInfo*>(&I) });
                break;
            case InfoKind::Friend:
                friends_.push_back({ actualAccess,
                    static_cast<const FriendInfo*>(&I) });
                break;
            default:
                MRDOCS_UNREACHABLE();
            }
        }
    }

    template<class T, class U>
    void
    sort(
        std::span<T const*> Interface::Tranche::*member,
        std::vector<T const*>& dest,
        Table<U>& src)
    {
        std::ranges::stable_sort(
            src,
            []( auto const& p0,
                auto const& p1) noexcept
            {
                return
                    to_underlying(p0.first) <
                    to_underlying(p1.first);
            });
        dest.resize(0);
        dest.reserve(src.size());
        for(auto const& v : src)
            dest.push_back(v.second);

        auto it0 = src.begin();
        auto it = std::find_if_not(
            it0, src.end(),
            [](auto const& p) noexcept
            {
                return p.first == AccessKind::Public;
            });
        std::size_t const nPublic = it - it0;
        it0 = it;
        it = std::find_if_not(
            it0, src.end(),
            [](auto const& p) noexcept
            {
                return p.first == AccessKind::Protected;
            });
        std::size_t const nPrivate = src.end() - it;
//MRDOCS_ASSERT(nPrivate == 0);
        I_.Public.*member    = { dest.begin(), dest.begin() + nPublic };
        I_.Protected.*member = { dest.begin() + nPublic, dest.end() - nPrivate };
        I_.Private.*member   = { dest.end() - nPrivate, dest.end() };
    }

    void
    finish()
    {
        sort(&Interface::Tranche::Records,          I_.records_,    records_);
        sort(&Interface::Tranche::Functions,        I_.functions_,  functions_);
        sort(&Interface::Tranche::Enums,            I_.enums_,      enums_);
        sort(&Interface::Tranche::Types,            I_.types_,      types_);
        sort(&Interface::Tranche::Data,             I_.data_,       data_);
        sort(&Interface::Tranche::StaticFunctions,  I_.staticfuncs_,staticfuncs_);
        sort(&Interface::Tranche::StaticData,       I_.staticdata_, staticdata_);
        sort(&Interface::Tranche::Friends,          I_.friends_,    friends_);
#if 0
MRDOCS_ASSERT(I_.Private.Records.empty());
MRDOCS_ASSERT(I_.Private.Functions.empty());
MRDOCS_ASSERT(I_.Private.Enums.empty());
MRDOCS_ASSERT(I_.Private.Types.empty());
MRDOCS_ASSERT(I_.Private.Data.empty());
MRDOCS_ASSERT(I_.Private.StaticFunctions.empty());
MRDOCS_ASSERT(I_.Private.StaticData.empty());
#endif
    }
};

Interface::
Interface(
    Corpus const& corpus_) noexcept
    : corpus(corpus_)
{
}

Interface
makeInterface(
    RecordInfo const& Derived,
    Corpus const& corpus)
{
    Interface I(corpus);
    Interface::Build(I, Derived, corpus);
    return I;
}

} // mrdocs
} // clang
