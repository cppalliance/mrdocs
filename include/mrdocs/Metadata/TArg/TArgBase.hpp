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

struct TArg
{
    /** The kind of template argument this is. */
    TArgKind Kind;

    /** Whether this template argument is a parameter expansion. */
    bool IsPackExpansion = false;

    constexpr virtual ~TArg() = default;

    constexpr bool isType()     const noexcept { return Kind == TArgKind::Type; }
    constexpr bool isNonType()  const noexcept { return Kind == TArgKind::NonType; }
    constexpr bool isTemplate() const noexcept { return Kind == TArgKind::Template; }

    auto operator<=>(TArg const&) const = default;

protected:
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
    static constexpr bool isNonType()  noexcept { return K == TArgKind::NonType; }
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
