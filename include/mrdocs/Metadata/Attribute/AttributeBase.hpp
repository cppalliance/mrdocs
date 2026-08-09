//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_ATTRIBUTE_ATTRIBUTEBASE_HPP
#define MRDOCS_API_METADATA_ATTRIBUTE_ATTRIBUTEBASE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Attribute/AttributeKind.hpp>
#include <mrdocs/Support/Reflection/CompareReflectedType.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <string>
#include <vector>

namespace mrdocs {

class DomCorpus;

/* Forward declarations
 */
#define INFO(PascalName) struct PascalName##Attribute;
#include <mrdocs/Metadata/Attribute/AttributeNodes.inc>

/** A C++ attribute attached to a symbol.

    This base class stores the information common to every
    attribute: the kind, the spelling name, and the raw
    balanced-token sequence written between the parentheses.
    Derived classes add the evaluated arguments for the
    attributes that take them.
*/
struct Attribute {
    /** The kind of attribute.
    */
    AttributeKind Kind;

    /** The attribute name as it appears in the standard form.

        For recognized attributes this is the normalized spelling
        (e.g. `deprecated`, `no_unique_address`). For unrecognized
        attributes (the `Other` kind) it is the spelling as written,
        including any scope (e.g. `gnu::custom`).
    */
    std::string Name;

    /** The attribute arguments as a balanced-token sequence.

        Each element is one top-level argument as rendered by Clang,
        e.g. `{"printf", "1", "2"}` for `[[gnu::format(printf, 1, 2)]]`.
        Empty for attributes that take no arguments. This is the raw
        token form; derived classes expose evaluated arguments.
    */
    std::vector<std::string> balancedTokens;

    /** View this instance as a const Attribute reference.
    */
    constexpr Attribute const& asAttribute() const noexcept
    {
        return *this;
    }

    /** View this instance as a mutable Attribute reference.
    */
    constexpr Attribute& asAttribute() noexcept
    {
        return *this;
    }

    #define INFO(PascalName) constexpr bool is##PascalName() const noexcept { \
        return Kind == AttributeKind::PascalName; \
    }
#include <mrdocs/Metadata/Attribute/AttributeNodes.inc>

#define INFO(PascalName) \
    constexpr PascalName##Attribute const& as##PascalName() const noexcept { \
        if (Kind == AttributeKind::PascalName) \
            return reinterpret_cast<PascalName##Attribute const&>(*this); \
        MRDOCS_UNREACHABLE(); \
    }
#include <mrdocs/Metadata/Attribute/AttributeNodes.inc>

#define INFO(PascalName) \
    constexpr PascalName##Attribute & as##PascalName() noexcept { \
        if (Kind == AttributeKind::PascalName) \
            return reinterpret_cast<PascalName##Attribute&>(*this); \
        MRDOCS_UNREACHABLE(); \
    }
#include <mrdocs/Metadata/Attribute/AttributeNodes.inc>

#define INFO(PascalName) \
    constexpr PascalName##Attribute const* as##PascalName##Ptr() const noexcept { \
        if (Kind == AttributeKind::PascalName) { return reinterpret_cast<PascalName##Attribute const*>(this); } \
        return nullptr; \
    }
#include <mrdocs/Metadata/Attribute/AttributeNodes.inc>

#define INFO(PascalName) \
    constexpr PascalName##Attribute * as##PascalName##Ptr() noexcept { \
        if (Kind == AttributeKind::PascalName) { return reinterpret_cast<PascalName##Attribute *>(this); } \
        return nullptr; \
    }
#include <mrdocs/Metadata/Attribute/AttributeNodes.inc>

protected:
    /** Virtual destructor for polymorphic base.
    */
    constexpr virtual ~Attribute() = default;

    /** Construct with a concrete attribute kind.
    */
    constexpr explicit Attribute(AttributeKind kind) noexcept
        : Kind(kind)
    {
    }
};

MRDOCS_DESCRIBE_STRUCT(
    Attribute,
    (),
    (Kind, Name, balancedTokens)
)


/** CRTP base that ties a concrete attribute to a fixed AttributeKind.
*/
template<AttributeKind K>
struct AttributeCommonBase : Attribute {
    /** Static discriminator for the concrete attribute.
    */
    static constexpr AttributeKind kind_id = K;

    #define INFO(PascalName) \
    static constexpr bool is##PascalName() noexcept { return K == AttributeKind::PascalName; }
#include <mrdocs/Metadata/Attribute/AttributeNodes.inc>

    MRDOCS_DESCRIBE_CLASS(AttributeCommonBase, (Attribute), ())

protected:
    /** Construct the base with the fixed kind.
    */
    constexpr AttributeCommonBase() noexcept
        : Attribute(K)
    {
    }
};

/** Compare two polymorphic attributes by visitor dispatch.
*/
MRDOCS_DECL
std::strong_ordering
operator<=>(Polymorphic<Attribute> const&, Polymorphic<Attribute> const&);

/** Equality for two polymorphic attributes.

    Declared here, next to `operator<=>`, and defined out of line in
    Attributes.cpp. It must be visible wherever `Polymorphic<Attribute>` is
    compared, so that `equality_comparable<Polymorphic<Attribute>>` holds
    (for example when an `Optional<Polymorphic<Attribute>>` member is
    compared through the reflected `operator<=>`). Keeping the body out of
    line also keeps it away from the kind registration hazard that a
    header-inline body would hit (see Attributes.cpp).
*/
MRDOCS_DECL
bool
operator==(Polymorphic<Attribute> const&, Polymorphic<Attribute> const&);

} // mrdocs

#endif // MRDOCS_API_METADATA_ATTRIBUTE_ATTRIBUTEBASE_HPP
