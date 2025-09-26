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

#ifndef MRDOCS_API_METADATA_INFO_RECORDBASE_HPP
#define MRDOCS_API_METADATA_INFO_RECORDBASE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Specifiers/AccessKind.hpp>
#include <mrdocs/Metadata/Type.hpp>

namespace clang::mrdocs {

/** Metadata for a direct base.
*/
struct BaseInfo
{
    /** The base type.

        This is typically a `NamedTypeInfo` that refers to a
        `RecordInfo`, but it could also be a more complex type
        such as a `decltype`.
     */
    Polymorphic<TypeInfo> Type;

    /** The access specifier for the base.
     */
    AccessKind Access = AccessKind::Public;

    /** Whether the base is virtual.
     */
    bool IsVirtual = false;

    BaseInfo() = delete;

    BaseInfo(
        Polymorphic<TypeInfo>&& type,
        AccessKind const access,
        bool const is_virtual)
        : Type(std::move(type))
        , Access(access)
        , IsVirtual(is_virtual)
    {
    }
};

MRDOCS_DECL
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    BaseInfo const& I,
    DomCorpus const* domCorpus);

} // clang::mrdocs

#endif // MRDOCS_API_METADATA_INFO_RECORDBASE_HPP
