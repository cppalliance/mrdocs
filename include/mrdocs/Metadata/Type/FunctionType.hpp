//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TYPE_FUNCTIONTYPE_HPP
#define MRDOCS_API_METADATA_TYPE_FUNCTIONTYPE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Specifiers.hpp>
#include <mrdocs/Metadata/Type/AutoType.hpp>
#include <mrdocs/Metadata/Type/TypeBase.hpp>
#include <mrdocs/Support/Describe.hpp>

namespace mrdocs {

/** Function type with parameters, qualifiers, and noexcept info.
*/
struct FunctionType final
    : TypeCommonBase<TypeKind::Function>
{
    /** Return type, defaults to `auto` when unknown.
    */
    Polymorphic<Type> ReturnType = Polymorphic<Type>(AutoType{});
    /** Parameter types in declaration order.
    */
    std::vector<Polymorphic<Type>> ParamTypes;
    /** Reference qualifier (if present).
    */
    ReferenceKind RefQualifier = ReferenceKind::None;
    /** Exception specification information.
    */
    NoexceptInfo ExceptionSpec;
    /** True when this function type is variadic.
    */
    bool IsVariadic = false;

};

MRDOCS_DESCRIBE_STRUCT(
    FunctionType,
    (Type),
    (ReturnType, ParamTypes, RefQualifier, ExceptionSpec, IsVariadic)
)

} // mrdocs

#endif // MRDOCS_API_METADATA_TYPE_FUNCTIONTYPE_HPP
