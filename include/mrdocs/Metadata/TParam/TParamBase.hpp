//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TPARAM_TPARAMBASE_HPP
#define MRDOCS_API_METADATA_TPARAM_TPARAMBASE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/TArg/TArgBase.hpp>
#include <mrdocs/Metadata/TParam/TParamKind.hpp>
#include <string>

namespace clang::mrdocs {

class DomCorpus;

/* Forward declarations
 */
#define INFO(Type) struct Type##TParam;
#include <mrdocs/Metadata/TParam/TParamInfoNodes.inc>

struct TParam
{
    /** The kind of template parameter this is
     */
    TParamKind Kind = TParamKind::Type;

    /** The template parameters name, if any
     */
    std::string Name;

    /** Whether this template parameter is a parameter pack */
    bool IsParameterPack = false;

    /** The default template argument, if any
     */
    Optional<Polymorphic<TArg>> Default = std::nullopt;

    constexpr virtual ~TParam() = default;

    std::strong_ordering operator<=>(TParam const&) const;

    constexpr TParam const& asTParam() const noexcept
    {
        return *this;
    }

    constexpr TParam& asTParam() noexcept
    {
        return *this;
    }

    #define INFO(Type) constexpr bool is##Type() const noexcept { \
        return Kind == TParamKind::Type; \
    }
#include <mrdocs/Metadata/TParam/TParamInfoNodes.inc>

    #define INFO(Type) \
    constexpr Type##TParam const& as##Type() const noexcept { \
        if (Kind == TParamKind::Type) \
            return reinterpret_cast<Type##TParam const&>(*this); \
        MRDOCS_UNREACHABLE(); \
    }
#include <mrdocs/Metadata/TParam/TParamInfoNodes.inc>

#define INFO(Type) \
    constexpr Type##TParam & as##Type() noexcept { \
        if (Kind == TParamKind::Type) \
            return reinterpret_cast<Type##TParam&>(*this); \
        MRDOCS_UNREACHABLE(); \
    }
#include <mrdocs/Metadata/TParam/TParamInfoNodes.inc>

#define INFO(Type) \
    constexpr Type##TParam const* as##Type##Ptr() const noexcept { \
        if (Kind == TParamKind::Type) { return reinterpret_cast<Type##TParam const*>(this); } \
        return nullptr; \
    }
#include <mrdocs/Metadata/TParam/TParamInfoNodes.inc>

#define INFO(Type) \
    constexpr Type##TParam * as##Type##Ptr() noexcept { \
        if (Kind == TParamKind::Type) { return reinterpret_cast<Type##TParam *>(this); } \
        return nullptr; \
    }
#include <mrdocs/Metadata/TParam/TParamInfoNodes.inc>


protected:
    constexpr
    TParam() noexcept = default;

    constexpr
    TParam(
        TParamKind kind) noexcept
        : Kind(kind)
    {
    }
};

void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    TParam const& I,
    DomCorpus const* domCorpus);

template<TParamKind K>
struct TParamCommonBase : TParam
{
    static constexpr TParamKind kind_id = K;

    static constexpr bool isType()     noexcept { return K == TParamKind::Type; }
    static constexpr bool isConstant() noexcept { return K == TParamKind::Constant; }
    static constexpr bool isTemplate() noexcept { return K == TParamKind::Template; }

    auto operator<=>(TParamCommonBase const&) const = default;

protected:
    constexpr
    TParamCommonBase() noexcept
        : TParam(K)
    {
    }
};

} // clang::mrdocs

#endif
