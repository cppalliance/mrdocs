//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_FIELD_HPP
#define MRDOCS_API_METADATA_FIELD_HPP

#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Source.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <mrdocs/Dom/LazyArray.hpp>

namespace clang::mrdocs {

/** Info for fields (i.e. non-static data members)

    Non-static data members cannot be redeclared.
*/
struct FieldInfo final
    : InfoCommonBase<InfoKind::Field>
{
    /** Type of the field */
    Polymorphic<TypeInfo> Type;

    /** The default member initializer, if any.
    */
    ExprInfo Default;

    /** Whether the field is a variant member */
    bool IsVariant = false;

    /** Whether the field is declared mutable */
    bool IsMutable = false;

    /** Whether the field is a bitfield */
    bool IsBitfield = false;

    /** The width of the bitfield */
    ConstantExprInfo<std::uint64_t> BitfieldWidth;

    //--------------------------------------------

    explicit FieldInfo(SymbolID ID) noexcept
        : InfoCommonBase(ID)
    {
    }
};

MRDOCS_DECL
void
merge(FieldInfo& I, FieldInfo&& Other);

/** Map a FieldInfo to a dom::Object.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    FieldInfo const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Info const&>(I), domCorpus);
    io.map("type", I.Type);
    if (!I.Default.Written.empty())
    {
        io.map("default", I.Default.Written);
    }
    io.map("isVariant", I.IsVariant);
    io.map("isMutable", I.IsMutable);
    io.map("isBitfield", I.IsBitfield);
    if (I.IsBitfield)
    {
        io.map("bitfieldWidth", I.BitfieldWidth.Written);
    }
}

/** Map the FieldInfo to a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    FieldInfo const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // clang::mrdocs

#endif
