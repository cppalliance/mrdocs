//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_NAME_IDENTIFIERNAMEINFO_HPP
#define MRDOCS_API_METADATA_NAME_IDENTIFIERNAMEINFO_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Name/NameBase.hpp>

namespace clang::mrdocs {

/** Represents an identifier

    This class is used to represent an identifier
    that could be in the corpus or not.

    When the symbol is in the corpus, the `id`
    field will be set to the symbol ID of the
    symbol.

    When the symbol is not in the corpus,
    the `id` field will be set to `SymbolID::invalid`.
*/
struct IdentifierNameInfo final
    : NameInfo
{
    constexpr
    IdentifierNameInfo() noexcept
        : NameInfo(NameKind::Identifier)
    {}

    auto
    operator<=>(IdentifierNameInfo const& other) const
    {
        return asName() <=> other.asName();
    }
};

} // clang::mrdocs

#endif
