//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Metadata/Attributes.hpp>
#include <mrdocs/Support/Reflection/CompareReflectedType.hpp>

namespace mrdocs {


std::strong_ordering
operator<=>(Polymorphic<Attribute> const& lhs, Polymorphic<Attribute> const& rhs)
{
    MRDOCS_ASSERT(!lhs.valueless_after_move());
    MRDOCS_ASSERT(!rhs.valueless_after_move());
    auto& lhsRef = *lhs;
    auto& rhsRef = *rhs;
    if (lhsRef.Kind == rhsRef.Kind)
    {
        return visit(lhsRef, detail::VisitCompareFn<Attribute>(rhsRef));
    }
    return lhsRef.Kind <=> rhsRef.Kind;
}

// Defined out of line, not inline in a header: the body resolves `lhs <=>
// rhs`, which drives the visitor comparison and `has_describe_kinds<Attribute>`.
// This translation unit includes Attributes.hpp, so the kinds are already
// registered here; a header-inline body could be parsed before the
// registration and cache the trait as false (Clang <= 19).
bool
operator==(Polymorphic<Attribute> const& lhs, Polymorphic<Attribute> const& rhs)
{
    return std::is_eq(lhs <=> rhs);
}

} // mrdocs
