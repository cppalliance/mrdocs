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

#ifndef MRDOCS_API_METADATA_TARG_HPP
#define MRDOCS_API_METADATA_TARG_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/TArg/ConstantTArg.hpp>
#include <mrdocs/Metadata/TArg/TArgBase.hpp>
#include <mrdocs/Metadata/TArg/TemplateTArg.hpp>
#include <mrdocs/Metadata/TArg/TypeTArg.hpp>
#include <mrdocs/Support/TypeTraits/Visitor.hpp>

namespace mrdocs {

// Register TArg's concrete kinds for the generic visit
// (Support/Reflection/Describe.hpp).
#define INFO(X) MRDOCS_KIND_ENTRY(TArg, X##TArg)
MRDOCS_DESCRIBE_KINDS_BEGIN(TArg)
#include <mrdocs/Metadata/TArg/TArgInfoNodes.inc>
MRDOCS_DESCRIBE_KINDS_END(TArg)
#undef INFO

/** Compare polymorphic template arguments.
*/
MRDOCS_DECL
std::strong_ordering
operator<=>(Polymorphic<TArg> const& lhs, Polymorphic<TArg> const& rhs);

/** Equality for polymorphic template arguments.
*/
inline bool
operator==(Polymorphic<TArg> const& a, Polymorphic<TArg> const& b)
{
    return std::is_eq(a <=> b);
}


/** Compare two template arguments for equality.

    A type argument is compared with @ref isEqual, so cv-qualifiers are
    significant and arrays do not decay; a constant argument is compared
    by its written value.

    @param lhs One template argument
    @param rhs The other template argument
    @param sameName How two names are judged equal
    @return Whether the two template arguments are equal
*/
MRDOCS_DECL
bool
isEqual(
    Polymorphic<TArg> const& lhs,
    Polymorphic<TArg> const& rhs,
    NameEquality const& sameName);

/// @copydoc isEqual(Polymorphic<TArg> const&, Polymorphic<TArg> const&, NameEquality const&)
MRDOCS_DECL
bool
isEqual(
    Polymorphic<TArg> const& lhs,
    Polymorphic<TArg> const& rhs);

} // mrdocs

#endif
