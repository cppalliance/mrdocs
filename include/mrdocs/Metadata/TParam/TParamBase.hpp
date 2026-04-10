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
#include <mrdocs/Support/CompareReflectedType.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>

namespace mrdocs {

class DomCorpus;

/* Forward declarations
 */
#define INFO(Type) struct Type##TParam;
#include <mrdocs/Metadata/TParam/TParamInfoNodes.inc>

/** Base class for a template parameter declaration.
*/
struct TParam
{
    /** The kind of template parameter this is
    */
    TParamKind Kind = TParamKind::Type;

    /** The template parameters name, if any
    */
    std::string Name;

    /** Whether this template parameter is a parameter pack
    */
    bool IsParameterPack = false;

    /** The default template argument, if any
    */
    Optional<Polymorphic<TArg>> Default = std::nullopt;

    /** Polymorphic base needs a virtual destructor.
    */
    constexpr virtual ~TParam() = default;

    /** View this object as a TParam reference.
    */
    constexpr TParam const& asTParam() const noexcept
    {
        return *this;
    }

    /** View this object as a mutable TParam reference.
    */
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
    /** Defaulted base constructor.
    */
    constexpr
    TParam() noexcept = default;

    /** Construct with a fixed parameter kind.
    */
    constexpr
    TParam(
        TParamKind kind) noexcept
        : Kind(kind)
    {
    }
};

MRDOCS_DESCRIBE_STRUCT(TParam, (), (Kind, Name, IsParameterPack, Default))

/** Serialize a template parameter into a DOM value.
*/
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    TParam const& I,
    DomCorpus const* domCorpus);

/** CRTP base that fixes the parameter kind.
*/
template<TParamKind K>
struct TParamCommonBase : TParam
{
    /** Static discriminator for the concrete parameter.
    */
    static constexpr TParamKind kind_id = K;

    /** True if the parameter is a type parameter.
        @return `true` when `kind_id` equals `TParamKind::Type`.
    */
    static constexpr bool isType()     noexcept { return K == TParamKind::Type; }

    /** True if the parameter is a non-type parameter.
        @return `true` when `kind_id` equals `TParamKind::Constant`.
    */
    static constexpr bool isConstant() noexcept { return K == TParamKind::Constant; }

    /** True if the parameter is a template template parameter.
        @return `true` when `kind_id` equals `TParamKind::Template`.
    */
    static constexpr bool isTemplate() noexcept { return K == TParamKind::Template; }

    MRDOCS_DESCRIBE_CLASS(TParamCommonBase, (TParam), ())

protected:
    /** Construct with the fixed kind.
    */
    constexpr
    TParamCommonBase() noexcept
        : TParam(K)
    {
    }
};

} // mrdocs

#endif
