//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_ATTRIBUTES_HPP
#define MRDOCS_API_METADATA_ATTRIBUTES_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Attribute/AssumeAttribute.hpp>
#include <mrdocs/Metadata/Attribute/AttributeBase.hpp>
#include <mrdocs/Metadata/Attribute/AttributeKind.hpp>
#include <mrdocs/Metadata/Attribute/CarriesDependencyAttribute.hpp>
#include <mrdocs/Metadata/Attribute/DeprecatedAttribute.hpp>
#include <mrdocs/Metadata/Attribute/FallthroughAttribute.hpp>
#include <mrdocs/Metadata/Attribute/IndeterminateAttribute.hpp>
#include <mrdocs/Metadata/Attribute/LikelyAttribute.hpp>
#include <mrdocs/Metadata/Attribute/MaybeUnusedAttribute.hpp>
#include <mrdocs/Metadata/Attribute/NoUniqueAddressAttribute.hpp>
#include <mrdocs/Metadata/Attribute/NodiscardAttribute.hpp>
#include <mrdocs/Metadata/Attribute/NoreturnAttribute.hpp>
#include <mrdocs/Metadata/Attribute/OtherAttribute.hpp>
#include <mrdocs/Metadata/Attribute/UnlikelyAttribute.hpp>
#include <mrdocs/Support/Visitor.hpp>

namespace mrdocs {

/** Visit an @ref Attribute with the provided callable.

    @param info The attribute instance to visit.
    @param fn The callable to dispatch to the concrete attribute.
    @param args Additional arguments forwarded to the callable.
    @return Whatever the callable returns.
*/
template<
    std::derived_from<Attribute> AttributeTy,
    class F,
    class... Args>
decltype(auto)
visit(
    AttributeTy& info,
    F&& fn,
    Args&&... args)
{
    auto visitor = makeVisitor<Attribute>(
        info, std::forward<F>(fn),
        std::forward<Args>(args)...);
    switch(info.Kind)
    {
    #define INFO(PascalName) case AttributeKind::PascalName: \
        return visitor.template visit<PascalName##Attribute>();
#include <mrdocs/Metadata/Attribute/AttributeNodes.inc>
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Serialize a polymorphic attribute into a DOM value.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Polymorphic<Attribute> const& I,
    DomCorpus const* domCorpus)
{
    MRDOCS_ASSERT(!I.valueless_after_move());
    tag_invoke(dom::ValueFromTag{}, v, *I, domCorpus);
}

} // mrdocs

#endif // MRDOCS_API_METADATA_ATTRIBUTES_HPP
