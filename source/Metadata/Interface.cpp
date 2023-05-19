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

    std::vector<std::pair<Access, DataMember>> data_;
    std::vector<std::pair<Access, MemberEnum>> enums_;
    std::vector<std::pair<Access, MemberFunction>> functions_;
    std::vector<std::pair<Access, MemberRecord>> records_;
    std::vector<std::pair<Access, MemberType>> types_;
    std::vector<std::pair<Access, StaticDataMember>> vars_;

public:
    Build(
        Interface& I,
        RecordInfo const& Derived,
        Corpus const& corpus) noexcept
        : I_(I)
        , corpus_(corpus)
        , includePrivate_(corpus_.config.includePrivate)
    {
        append(Access::Public, Derived);

        finish();
    }

private:
    static
    Access
    effectiveAccess(
        Access t0,
        Access t1) noexcept
    {
        if( t0 ==  Access::Private ||
            t1 ==  Access::Private)
            return Access::Private;

        if( t0 ==  Access::Protected ||
            t1 ==  Access::Protected)
            return Access::Protected;

        return Access::Public;
    }

    void
    append(
        Access access,
        RecordInfo const& From)
    {
        for(auto const& B : From.Bases)
        {
            auto actualAccess = effectiveAccess(access, B.access);
            append(actualAccess, corpus_.get<RecordInfo>(B.id));
        }

        // Data Members
        if( includePrivate_ ||
            access != Access::Private)
        {
            for(auto const& J : From.Members)
            {
                auto actualAccess = effectiveAccess(access, J.access);
                data_.push_back({ actualAccess, { &J, &From } });
            }
        }

        // Member Enums
        if( includePrivate_ ||
            access != Access::Private)
        {
            for(auto const& ref : From.Children_.Enums)
            {
                auto const& J = corpus_.get<EnumInfo>(ref.id);
                auto actualAccess = effectiveAccess(access, ref.access);
                enums_.push_back({ actualAccess, { &J, &From } });
            }
        }

        // Member Functions
        {
            auto const isRecFinal = From.specs.isFinal.get();
            for(auto const& ref : From.Children_.Functions)
            {
                auto const& J = corpus_.get<FunctionInfo>(ref.id);
                auto actualAccess = effectiveAccess(access, ref.access);
                //bool isFinal = J.specs0.isFinal;
                // private virtual functions are effectively public
                // and should be emitted unless the record is final
                if( includePrivate_ ||
                    actualAccess != Access::Private ||
                    ( ! isRecFinal && J.specs0.isVirtual ))
                {
                    functions_.push_back({ actualAccess, { &J, &From }});
                }
            }
        }

        // Member Records
        if( includePrivate_ ||
            access != Access::Private)
        {
            for(auto const& ref : From.Children_.Records)
            {
                auto const& J = corpus_.get<RecordInfo>(ref.id);
                auto actualAccess = effectiveAccess(access, ref.access);
                records_.push_back({ actualAccess, { &J, &From }});
            }
        }

        // Member Types
        if( includePrivate_ ||
            access != Access::Private)
        {
            for(auto const& ref : From.Children_.Types)
            {
                auto const& J = corpus_.get<TypedefInfo>(ref.id);
                auto actualAccess = effectiveAccess(access, ref.access);
                types_.push_back({ actualAccess, { &J, &From }});
            }
        }

        // Static Data Members
        if( includePrivate_ ||
            access != Access::Private)
        {
            for(auto const& ref : From.Children_.Vars)
            {
                auto const& J = corpus_.get<VarInfo>(ref.id);
                auto actualAccess = effectiveAccess(access, ref.access);
                vars_.push_back({ actualAccess, { &J, &From }});
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
                return p.first == Access::Public;
            });
        std::size_t const nPublic = it - it0;
        it0 = it;
        it = std::find_if_not(
            it0, src.end(),
            [](auto const& p) noexcept
            {
                return p.first == Access::Protected;
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
