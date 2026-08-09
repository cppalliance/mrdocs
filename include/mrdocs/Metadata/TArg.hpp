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


} // mrdocs

#endif
