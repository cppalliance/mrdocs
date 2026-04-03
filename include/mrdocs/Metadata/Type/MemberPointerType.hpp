//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TYPE_MEMBERPOINTERTYPE_HPP
#define MRDOCS_API_METADATA_TYPE_MEMBERPOINTERTYPE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Type/TypeBase.hpp>
#include <mrdocs/Support/Describe.hpp>

namespace mrdocs {

/** Pointer-to-member type (object or function).
*/
struct MemberPointerType final
    : TypeCommonBase<TypeKind::MemberPointer>
{
    /** Containing class type.
    */
    Polymorphic<Type> ParentType = Polymorphic<Type>(AutoType{});

    /** Pointee type being referenced.
    */
    Polymorphic<Type> PointeeType = Polymorphic<Type>(AutoType{});

    /** Compare member pointers by parent and pointee.
    */
    std::strong_ordering
    operator<=>(MemberPointerType const&) const;
};

MRDOCS_DESCRIBE_STRUCT(MemberPointerType, (TypeCommonBase<TypeKind::MemberPointer>), (ParentType, PointeeType))

} // mrdocs

#endif // MRDOCS_API_METADATA_TYPE_MEMBERPOINTERTYPE_HPP
