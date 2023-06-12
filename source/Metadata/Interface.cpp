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

#include "Support/Debug.hpp"
#include "Support/TypeTraits.hpp"
#include <mrdox/Metadata/Interface.hpp>
#include <mrdox/Config.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/Metadata/Enum.hpp>
#include <mrdox/Metadata/Function.hpp>
#include <mrdox/Metadata/Record.hpp>
#include <mrdox/Metadata/Typedef.hpp>
#include <mrdox/Metadata/Var.hpp>
#include <algorithm>

namespace clang {
namespace mrdox {

//------------------------------------------------

class Interface::Build
{
    Interface& I_;
    Corpus const& corpus_;
    bool includePrivate_;

    std::vector<std::pair<AccessKind, MemberEnum>> enums_;
    std::vector<std::pair<AccessKind, MemberFunction>> functions_;
    std::vector<std::pair<AccessKind, MemberRecord>> records_;
    std::vector<std::pair<AccessKind, MemberType>> types_;
    std::vector<std::pair<AccessKind, DataMember>> data_;
    std::vector<std::pair<AccessKind, StaticDataMember>> vars_;

public:
    Build(
        Interface& I,
        RecordInfo const& Derived,
        Corpus const& corpus) noexcept
        : I_(I)
        , corpus_(corpus)
        , includePrivate_(corpus_.config.includePrivate)
    {
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
            auto actualAccess = effectiveAccess(access, B.access);
            // VFALCO temporary hack to avoid looking up IDs
            //        which for metadata that is not emitted.
            if(! corpus_.find(B.id))
                continue;
            append(actualAccess, corpus_.get<RecordInfo>(B.id));
        }

        if( includePrivate_ ||
            access != AccessKind::Private)
        {
            for(auto const& id : From.Members)
            {
                const auto& I = corpus_.get<Info>(id);
                auto actualAccess = effectiveAccess(access, I.Access);
                switch(I.Kind)
                {
                case InfoKind::Enum:
                    enums_.push_back({ actualAccess,
                        { static_cast<const EnumInfo*>(&I), &From } });
                    break;
                case InfoKind::Field:
                    data_.push_back({ actualAccess,
                        { static_cast<const FieldInfo*>(&I), &From } });
                    break;
                case InfoKind::Function:
                {
                    auto const isRecFinal = From.specs.isFinal.get();
                    const auto& F = static_cast<const FunctionInfo&>(I);
                    if( includePrivate_ ||
                        actualAccess != AccessKind::Private ||
                        ( ! isRecFinal && F.specs0.isVirtual ))
                    {
                        functions_.push_back({ actualAccess, { &F, &From } });
                    }
                    break;
                }
                case InfoKind::Record:
                    records_.push_back({ actualAccess,
                        { static_cast<const RecordInfo*>(&I), &From } });
                    break;
                case InfoKind::Typedef:
                    types_.push_back({ actualAccess,
                        { static_cast<const TypedefInfo*>(&I), &From } });
                    break;
                case InfoKind::Variable:
                    vars_.push_back({ actualAccess,
                        { static_cast<const VarInfo*>(&I), &From } });
                    break;

                default:
                    break;
                }
            }
        }
    }

    template<class T, class U>
    void
    sort(
        std::span<T const> Interface::Tranche::*member,
        std::vector<T>& dest,
        std::vector<U>& src)
    {
        llvm::stable_sort(
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

        I_.Public.*member    = { dest.begin(), dest.begin() + nPublic };
        I_.Protected.*member = { dest.begin() + nPublic, dest.end() - nPrivate };
        I_.Private.*member   = { dest.end() - nPrivate, dest.end() };
    }

    void
    finish()
    {
        sort(&Interface::Tranche::Records,   I_.records_,   records_);
        sort(&Interface::Tranche::Functions, I_.functions_, functions_);
        sort(&Interface::Tranche::Enums,     I_.enums_,     enums_);
        sort(&Interface::Tranche::Types,     I_.types_,     types_);
        sort(&Interface::Tranche::Data,      I_.data_,      data_);
        sort(&Interface::Tranche::Vars,      I_.vars_,      vars_);
    }
};

Interface&
makeInterface(
    Interface& I,
    RecordInfo const& Derived,
    Corpus const& corpus)
{
    Interface::Build(I, Derived, corpus);
    return I;
}

} // mrdox
} // clang
