//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SYMBOL_RECORDBASE_HPP
#define MRDOCS_API_METADATA_SYMBOL_RECORDBASE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Specifiers/AccessKind.hpp>
#include <mrdocs/Metadata/Symbol/Source.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>

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

    /** Location of the base-specifier in the derived class.
    */
    SourceInfo Loc;

    /** Bases must be explicitly described.
    */
    BaseInfo() = delete;

    /** Create a base description.
        @param type The base type.
        @param access Declared access specifier.
        @param is_virtual Whether the base is virtual.
        @param loc Location of the base-specifier in the derived class.
    */
    BaseInfo(
        Polymorphic<struct Type>&& type,
        AccessKind const access,
        bool const is_virtual,
        SourceInfo loc = {})
        : Type(std::move(type))
        , Access(access)
        , IsVirtual(is_virtual)
        , Loc(std::move(loc))
    {
    }
};

MRDOCS_DESCRIBE_STRUCT(
    BaseInfo,
    (),
    (Type, Access, IsVirtual, Loc)
)


} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_RECORDBASE_HPP
