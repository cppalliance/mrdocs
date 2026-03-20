//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SYMBOL_RECORDBASE_HPP
#define MRDOCS_API_METADATA_SYMBOL_RECORDBASE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Specifiers/AccessKind.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <mrdocs/Support/Describe.hpp>

namespace mrdocs {

/** Metadata for a direct base.
*/
struct BaseInfo
{
    /** The base type.

        This is typically a `NamedType` that refers to a
        `RecordSymbol`, but it could also be a more complex type
        such as a `decltype`.
    */
    Polymorphic<struct Type> Type;

    /** The access specifier for the base.
    */
    AccessKind Access = AccessKind::Public;

    /** Whether the base is virtual.
    */
    bool IsVirtual = false;

    /** Bases must be explicitly described.
    */
    BaseInfo() = delete;

    /** Create a base description.
        @param type The base type.
        @param access Declared access specifier.
        @param is_virtual Whether the base is virtual.
    */
    BaseInfo(
        Polymorphic<struct Type>&& type,
        AccessKind const access,
        bool const is_virtual)
        : Type(std::move(type))
        , Access(access)
        , IsVirtual(is_virtual)
    {
    }
};

MRDOCS_DESCRIBE_STRUCT(
    BaseInfo,
    (),
    (Type, Access, IsVirtual)
)

/** Serialize a base description into a DOM value.
*/
MRDOCS_DECL
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    BaseInfo const& I,
    DomCorpus const* domCorpus);

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_RECORDBASE_HPP
