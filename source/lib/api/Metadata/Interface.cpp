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

#include "api/Support/Debug.hpp"
#include "api/Support/TypeTraits.hpp"
#include <mrdox/Metadata/Interface.hpp>
#include <mrdox/Config.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/Metadata/Enum.hpp>
#include <mrdox/Metadata/Function.hpp>
#include <mrdox/Metadata/Record.hpp>
#include <mrdox/Metadata/Typedef.hpp>
#include <mrdox/Metadata/Var.hpp>

namespace clang {
namespace mrdox {

class Interface::Build
{
    Interface& I_;
    Corpus const& corpus_;
    bool includePrivate_;

public:
    Build(
        Interface& I,
        RecordInfo const& Derived,
        Corpus const& corpus) noexcept
        : I_(I)
        , corpus_(corpus)
        , includePrivate_(corpus_.config.includePrivate)
    {
        I_.records_.clear();
        I_.functions_.clear();
        I_.enums_.clear();
        I_.types_.clear();
        I_.data_.clear();
        I_.vars_.clear();
        append(Access::Public, Derived);
        finish();
    }

private:
    static
    Access
    effectiveAccess(
        Access t0,
        AccessSpecifier t1) noexcept
    {
        if( t1 ==  AccessSpecifier::AS_none)
            return t0;
        if( t0 ==  Access::Private ||
            t1 ==  AccessSpecifier::AS_private)
            return Access::Private;
        if( t0 ==  Access::Protected ||
            t1 ==  AccessSpecifier::AS_protected)
            return Access::Protected;
        return Access::Public;
    }

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
            auto actualAccess = effectiveAccess(access, B.Access);
            append(actualAccess, corpus_.get<RecordInfo>(B.id));
        }

        // Records
        if( includePrivate_ ||
            access != Access::Private)
        {
            for(auto const& ref : From.Children_.Records)
            {
                auto const& J = corpus_.get<RecordInfo>(ref.id);
                auto actualAccess = effectiveAccess(access, ref.access);
                I_.records_.push_back({ &J, &From, actualAccess });
            }
        }

        // Functions
        {
            auto const isFinal = From.specs.isFinal.get();
            for(auto const& ref : From.Children_.Functions)
            {
                auto const& J = corpus_.get<FunctionInfo>(ref.id);
                auto actualAccess = effectiveAccess(access, J.Access);
                // private virtual functions are emitted anyway since
                // they are always accessible from derived classes
                // unless this class is marked `final`.
                if( includePrivate_ ||
                    actualAccess != Access::Private ||
                    ( ! isFinal && J.specs0.isVirtual ))
                {
                    I_.functions_.push_back({ &J, &From, actualAccess });
                }
            }
        }

        // Enums
        if( includePrivate_ ||
            access != Access::Private)
        {
            for(auto const& ref : From.Children_.Enums)
            {
                auto const& J = corpus_.get<EnumInfo>(ref.id);
                auto actualAccess = effectiveAccess(access, ref.access);
                I_.enums_.push_back({ &J, &From, actualAccess });
            }
        }

        // Typedefs
        if( includePrivate_ ||
            access != Access::Private)
        {
            for(auto const& ref : From.Children_.Types)
            {
                auto const& J = corpus_.get<TypedefInfo>(ref.id);
                auto actualAccess = effectiveAccess(access, ref.access);
                I_.types_.push_back({ &J, &From, actualAccess });
            }
        }

        // Data Members
        if( includePrivate_ ||
            access != Access::Private)
        {
            for(auto const& J : From.Members)
            {
                auto actualAccess = effectiveAccess(access, J.Access);
                I_.data_.push_back({ &J, &From, actualAccess });
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
                I_.vars_.push_back({ &J, &From, actualAccess });
            }
        }
    }

    template<class T>
    void
    sort(
        std::span<T const> Interface::Tranche::*member,
        std::vector<T>& list)
    {
        llvm::stable_sort(
            list,
            []( auto const& I0,
                auto const& I1) noexcept
            {
                return
                    to_underlying(I0.access) <
                    to_underlying(I1.access);
            });
        auto it0 = list.begin();
        auto it = std::find_if_not(
            it0, list.end(),
            [](auto const& I) noexcept
            {
                return I.access == Access::Public;
            });
        I_.Public.*member = { it0, it };
        it0 = it;
        it = std::find_if(
            it0, list.end(),
            [](auto const& I) noexcept
            {
                return I.access == Access::Protected;
            });
        I_.Protected.*member = { it0, it };
        I_.Private.*member = { it, list.end() };
    }

    void
    finish()
    {
        sort(&Interface::Tranche::Records,   I_.records_);
        sort(&Interface::Tranche::Functions, I_.functions_);
        sort(&Interface::Tranche::Enums,     I_.enums_);
        sort(&Interface::Tranche::Types,     I_.types_);
        sort(&Interface::Tranche::Data,      I_.data_);
        sort(&Interface::Tranche::Vars,      I_.vars_);
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
