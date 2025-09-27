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

namespace clang::mrdocs {

/* Forward declarations
 */
#define INFO(Type) struct Type##TArg;
#include <mrdocs/Metadata/TArg/TArgInfoNodes.inc>

struct TArg
{
    /** The kind of template argument this is. */
    TArgKind Kind = TArgKind::Type;

    /** Whether this template argument is a parameter expansion. */
    bool IsPackExpansion = false;

    constexpr virtual ~TArg() = default;

    auto operator<=>(TArg const&) const = default;

    constexpr TArg const& asTArg() const noexcept
    {
        return *this;
    }

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
    constexpr TArg() noexcept = default;

    constexpr
    TArg(
        TArgKind kind) noexcept
        : Kind(kind)
    {
    }
};

template<TArgKind K>
struct TArgCommonBase : TArg
{
    static constexpr TArgKind kind_id = K;

    static constexpr bool isType()     noexcept { return K == TArgKind::Type; }
    static constexpr bool isConstant()  noexcept { return K == TArgKind::Constant; }
    static constexpr bool isTemplate() noexcept { return K == TArgKind::Template; }

protected:
    constexpr
    TArgCommonBase() noexcept
        : TArg(K)
    {
    }
};

MRDOCS_DECL
std::string
toString(TArg const& arg) noexcept;

MRDOCS_DECL
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    TArg const& I,
    DomCorpus const* domCorpus);

} // clang::mrdocs

#endif // MRDOCS_API_METADATA_TARG_TARGBASE_HPP
