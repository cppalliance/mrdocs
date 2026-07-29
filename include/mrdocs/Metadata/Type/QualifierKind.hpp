//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TYPE_QUALIFIERKIND_HPP
#define MRDOCS_API_METADATA_TYPE_QUALIFIERKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Metadata/Name/NameBase.hpp>
#include <mrdocs/Metadata/Specifiers.hpp>
#include <mrdocs/Metadata/Symbol/SymbolID.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <mrdocs/Support/TypeTraits/TypeTraits.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace mrdocs {

/** Type qualifiers
*/
enum class QualifierKind
{
    /// No qualifiers
    None,
    /// The const qualifier
    Const,
    /// The volatile qualifier
    Volatile,
    /// Both the const and volatile qualifiers
    ConstVolatile
};

MRDOCS_DESCRIBE_ENUM(
    QualifierKind,
    None, Const, Volatile, ConstVolatile)
MRDOCS_DESCRIBE_ENUM_UNDEFINED(QualifierKind, None)

/** Convert a cv qualifier kind to its string form.

    @param kind The qualifier kind.
    @return The written qualifier syntax; empty for QualifierKind::None.
*/
constexpr
std::string_view
toString(QualifierKind kind) noexcept
{
    switch (kind)
    {
    case QualifierKind::None:          return "";
    case QualifierKind::Const:         return "const";
    case QualifierKind::Volatile:      return "volatile";
    case QualifierKind::ConstVolatile: return "const volatile";
    default:                           return "";
    }
}

} // mrdocs

#endif
