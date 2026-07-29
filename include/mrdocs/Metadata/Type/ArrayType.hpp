//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TYPE_ARRAYTYPE_HPP
#define MRDOCS_API_METADATA_TYPE_ARRAYTYPE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Type/AutoType.hpp>
#include <mrdocs/Metadata/Type/TypeBase.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <mrdocs/Support/Reflection/MapReflectedType.hpp>

namespace mrdocs {

/** C++ array type (bounded or unbounded).
*/
struct ArrayType final
    : TypeCommonBase<TypeKind::Array>
{
    /** Element type held by the array.
    */
    Polymorphic<Type> ElementType = Polymorphic<Type>(AutoType{});

    /** Optional bound; empty means unknown or dependent.
    */
    ConstantExprInfo<std::uint64_t> Bounds;

};

MRDOCS_DESCRIBE_STRUCT(
    ArrayType,
    (Type),
    (ElementType, Bounds)
)

/** Map an ArrayType to a dom::Object with split bounds properties.
    @param io The IO object to map into.
    @param I The ArrayType to map.
    @param domCorpus The DomCorpus context.
*/
template <typename IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    ArrayType const& I,
    DomCorpus const* domCorpus)
{
    addMetaObject<ArrayType>(io);
    tag_invoke(dom::LazyObjectMapTag{}, io,
        static_cast<Type const&>(I), domCorpus);
    io.map("elementType", I.ElementType);
    if (I.Bounds.Value)
    {
        io.map("boundsValue", *I.Bounds.Value);
    }
    io.map("boundsExpr", I.Bounds.Written);
}

} // mrdocs

#endif // MRDOCS_API_METADATA_TYPE_ARRAYTYPE_HPP
