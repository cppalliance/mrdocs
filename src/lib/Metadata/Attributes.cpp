//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Dom/LazyObject.hpp>
#include <mrdocs/Metadata/Attributes.hpp>
#include <mrdocs/Support/CompareReflectedType.hpp>
#include <mrdocs/Support/MapReflectedType.hpp>

namespace mrdocs {

dom::String
toString(AttributeKind kind) noexcept
{
    switch(kind)
    {
    case AttributeKind::Other:
        return "other";
    case AttributeKind::Noreturn:
        return "noreturn";
    case AttributeKind::CarriesDependency:
        return "carries-dependency";
    case AttributeKind::Deprecated:
        return "deprecated";
    case AttributeKind::Fallthrough:
        return "fallthrough";
    case AttributeKind::MaybeUnused:
        return "maybe-unused";
    case AttributeKind::Nodiscard:
        return "nodiscard";
    case AttributeKind::Likely:
        return "likely";
    case AttributeKind::Unlikely:
        return "unlikely";
    case AttributeKind::NoUniqueAddress:
        return "no-unique-address";
    case AttributeKind::Assume:
        return "assume";
    case AttributeKind::Indeterminate:
        return "indeterminate";
    default:
        MRDOCS_UNREACHABLE();
    }
}

void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Attribute const& I,
    DomCorpus const* domCorpus)
{
    visit(I, [&]<typename T>(T const& t) {
        v = dom::LazyObject(t, domCorpus);
    });
}

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

} // mrdocs
