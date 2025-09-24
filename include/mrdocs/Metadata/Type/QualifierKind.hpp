//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TYPE_QUALIFIERKIND_HPP
#define MRDOCS_API_METADATA_TYPE_QUALIFIERKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Metadata/Info/SymbolID.hpp>
#include <mrdocs/Metadata/Name/NameBase.hpp>
#include <mrdocs/Metadata/Specifiers.hpp>
#include <mrdocs/Support/TypeTraits.hpp>
#include <string>
#include <vector>

namespace clang::mrdocs {

/** Type qualifiers
*/
enum QualifierKind
{
    /// No qualifiers
    None,
    /// The const qualifier
    Const,
    /// The volatile qualifier
    Volatile
};

MRDOCS_DECL
dom::String
toString(QualifierKind kind) noexcept;

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    QualifierKind kind)
{
    v = toString(kind);
}

} // clang::mrdocs

#endif
