//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_NAME_NAMEBASE_HPP
#define MRDOCS_API_METADATA_NAME_NAMEBASE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Name/NameKind.hpp>
#include <mrdocs/Metadata/Symbol/SymbolID.hpp>

namespace mrdocs {

/* Forward declarations
 */
#define INFO(Type) struct Type##Name;
#include <mrdocs/Metadata/Name/NameNodes.inc>

/** Represents a name for a named `Type`

    When the `Type` is a named type, this class
    represents the name of the type.

    It also includes the symbol ID of the named type,
    so that it can be referenced in the documentation.

    This allows the `Type` to store either a
    `Name` or a `SpecializationName`,
    which contains the arguments for a template specialization
    without requiring the application to extract an
    unnecessary symbol.
 */
struct Name
{
    /** The kind of name this is.
    */
    NameKind Kind;

    /** The SymbolID of the named symbol, if it exists.
    */
    SymbolID id = SymbolID::invalid;

    /** The unqualified name.
     */
    std::string Identifier;

    /** The parent name info, if any.

        This recursively includes information about
        the parent, such as the symbol ID and
        potentially template arguments, when
        the parent is a SpecializationName.

        This is particularly useful because the
        parent of `id` could be a primary template.
        In this case, the Prefix will contain
        this primary template information
        and the template arguments.

     */
    Optional<Polymorphic<struct Name>> Prefix = std::nullopt;

    constexpr virtual ~Name() = default;

    std::strong_ordering
    operator<=>(Name const& other) const;

    bool
    operator==(Name const& other) const
    {
        return std::is_eq(*this <=> other);
    }

    constexpr Name const& asName() const noexcept
    {
        return *this;
    }

    constexpr Name& asName() noexcept
    {
        return *this;
    }

    #define INFO(Type) constexpr bool is##Type() const noexcept { \
        return Kind == NameKind::Type; \
    }
#include <mrdocs/Metadata/Name/NameNodes.inc>

#define INFO(Type) \
    constexpr Type##Name const& as##Type() const noexcept { \
        if (Kind == NameKind::Type) \
            return reinterpret_cast<Type##Name const&>(*this); \
        MRDOCS_UNREACHABLE(); \
    }
#include <mrdocs/Metadata/Name/NameNodes.inc>

#define INFO(Type) \
    constexpr Type##Name & as##Type() noexcept { \
        if (Kind == NameKind::Type) \
            return reinterpret_cast<Type##Name&>(*this); \
        MRDOCS_UNREACHABLE(); \
    }
#include <mrdocs/Metadata/Name/NameNodes.inc>

#define INFO(Type) \
    constexpr Type##Name const* as##Type##Ptr() const noexcept { \
        if (Kind == NameKind::Type) { return reinterpret_cast<Type##Name const*>(this); } \
        return nullptr; \
    }
#include <mrdocs/Metadata/Name/NameNodes.inc>

#define INFO(Type) \
    constexpr Type##Name * as##Type##Ptr() noexcept { \
        if (Kind == NameKind::Type) { return reinterpret_cast<Type##Name *>(this); } \
        return nullptr; \
    }
#include <mrdocs/Metadata/Name/NameNodes.inc>

protected:
    constexpr
    Name() noexcept
        : Kind(NameKind::Identifier) {};

    explicit
        constexpr
        Name(NameKind const kind) noexcept
        : Kind(kind) {}
};

MRDOCS_DECL
std::string
toString(Name const& N);

MRDOCS_DECL
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Name const& I,
    DomCorpus const* domCorpus);

} // mrdocs

#endif
