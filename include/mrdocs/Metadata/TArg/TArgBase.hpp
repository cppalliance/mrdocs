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

#ifndef MRDOCS_API_METADATA_TARG_TARGBASE_HPP
#define MRDOCS_API_METADATA_TARG_TARGBASE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/TArg/TArgKind.hpp>
#include <mrdocs/Support/Describe.hpp>

namespace mrdocs {

/* Forward declarations
 */
#define INFO(Type) struct Type##TArg;
#include <mrdocs/Metadata/TArg/TArgInfoNodes.inc>

/** Base class for any template argument.
*/
struct TArg
{
    /** The kind of template argument this is.
    */
    TArgKind Kind = TArgKind::Type;

    /** Whether this template argument is a parameter expansion.
    */
    bool IsPackExpansion = false;

    /** Polymorphic base needs a virtual destructor.
    */
    constexpr virtual ~TArg() = default;

    /** Compare arguments by stored data.
    */
    auto operator<=>(TArg const&) const = default;

    /** View this object as a TArg reference.
    */
    constexpr TArg const& asTArg() const noexcept
    {
        return *this;
    }

    /** View this object as a mutable TArg reference.
    */
    constexpr TArg& asTArg() noexcept
    {
        return *this;
    }

    #define INFO(Type) constexpr bool is##Type() const noexcept { \
        return Kind == TArgKind::Type; \
    }
#include <mrdocs/Metadata/TArg/TArgInfoNodes.inc>

#define INFO(Type) \
    constexpr Type##TArg const& as##Type() const noexcept { \
        if (Kind == TArgKind::Type) \
            return reinterpret_cast<Type##TArg const&>(*this); \
        MRDOCS_UNREACHABLE(); \
    }
#include <mrdocs/Metadata/TArg/TArgInfoNodes.inc>

#define INFO(Type) \
    constexpr Type##TArg & as##Type() noexcept { \
        if (Kind == TArgKind::Type) \
            return reinterpret_cast<Type##TArg&>(*this); \
        MRDOCS_UNREACHABLE(); \
    }
#include <mrdocs/Metadata/TArg/TArgInfoNodes.inc>

#define INFO(Type) \
    constexpr Type##TArg const* as##Type##Ptr() const noexcept { \
        if (Kind == TArgKind::Type) { return reinterpret_cast<Type##TArg const*>(this); } \
        return nullptr; \
    }
#include <mrdocs/Metadata/TArg/TArgInfoNodes.inc>

#define INFO(Type) \
    constexpr Type##TArg * as##Type##Ptr() noexcept { \
        if (Kind == TArgKind::Type) { return reinterpret_cast<Type##TArg *>(this); } \
        return nullptr; \
    }
#include <mrdocs/Metadata/TArg/TArgInfoNodes.inc>

protected:
    /** Defaulted base constructor.
    */
    constexpr TArg() noexcept = default;

    /** Construct with a specific argument kind.
    */
    constexpr
    TArg(
        TArgKind kind) noexcept
        : Kind(kind)
    {
    }
};

MRDOCS_DESCRIBE_STRUCT(TArg, (), (Kind, IsPackExpansion))

/** CRTP base that fixes the argument kind.
*/
template<TArgKind K>
struct TArgCommonBase : TArg
{
    /** Static discriminator for the concrete argument.
    */
    static constexpr TArgKind kind_id = K;

    /** Test whether the kind is a type argument.
        @return `true` if `kind_id` equals `TypeKind::Type`.
    */
    static constexpr bool isType()     noexcept { return K == TArgKind::Type; }
    /** Test whether the kind is a non-type constant argument.
        @return `true` if `kind_id` equals `TypeKind::Constant`.
    */
    static constexpr bool isConstant()  noexcept { return K == TArgKind::Constant; }
    /** Test whether the kind is a template argument.
        @return `true` if `kind_id` equals `TypeKind::Template`.
    */
    static constexpr bool isTemplate() noexcept { return K == TArgKind::Template; }

    MRDOCS_DESCRIBE_CLASS(TArgCommonBase, (TArg), ())

protected:
    /** Construct with the fixed kind.
    */
    constexpr
    TArgCommonBase() noexcept
        : TArg(K)
    {
    }
};

/** Convert a template argument to a human-readable string.
    @return Descriptive text for the argument.
*/
MRDOCS_DECL
std::string
toString(TArg const& arg) noexcept;

/** Serialize the argument to a DOM value.
*/
MRDOCS_DECL
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    TArg const& I,
    DomCorpus const* domCorpus);

} // mrdocs

#endif // MRDOCS_API_METADATA_TARG_TARGBASE_HPP
