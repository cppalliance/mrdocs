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
#include <mrdocs/Metadata/Record.hpp>
#include <mrdocs/Metadata/Typedef.hpp>
#include <mrdocs/Metadata/Variable.hpp>
#include <algorithm>

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
        append(AccessKind::Public, Derived);

        finish();
    }

private:
    static
    AccessKind
    effectiveAccess(
        AccessKind t0,
        AccessKind t1) noexcept
    {
        if( t0 ==  AccessKind::Private ||
            t1 ==  AccessKind::Private)
            return AccessKind::Private;

        if( t0 ==  AccessKind::Protected ||
            t1 ==  AccessKind::Protected)
            return AccessKind::Protected;

        return AccessKind::Public;
    }

    void
    append(
        AccessKind access,
        RecordInfo const& From)
    {
        for(auto const& B : From.Bases)
        {
            auto actualAccess = effectiveAccess(access, B.Access);
            if( ! includePrivate_ &&
                actualAccess == AccessKind::Private)
                continue;
            // VFALCO temporary hack to avoid looking up IDs
            //        which for metadata that is not emitted
            continue;
#if 0
            if(! B.Type.id || ! corpus_.find(B.Type.id))
                continue;
            append(actualAccess,
                corpus_.get<RecordInfo>(B.Type.id));
#endif
        }
        for(auto const& id : From.Members)
        {
            const auto& I = corpus_.get<Info>(id);
            auto actualAccess = effectiveAccess(access, I.Access);
            if(I.Kind == InfoKind::Function)
            {
                const auto& F = static_cast<FunctionInfo const&>(I);
                auto const isRecFinal = From.specs.isFinal.get();
                if( includePrivate_ ||
                    actualAccess != AccessKind::Private || (
                        ! isRecFinal && F.specs0.isVirtual))
                {
                    if(F.specs0.storageClass == StorageClassKind::Static)
                        staticfuncs_.push_back({ actualAccess, &F });
                    else
                        functions_.push_back({ actualAccess, &F });
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
