//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SYMBOL_ENUMCONSTANT_HPP
#define MRDOCS_API_METADATA_SYMBOL_ENUMCONSTANT_HPP

#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Metadata/Symbol.hpp>
#include <mrdocs/Metadata/Symbol/Source.hpp>

namespace mrdocs {

/** Info for enum constants.
*/
struct EnumConstantSymbol final
    : SymbolCommonBase<SymbolKind::EnumConstant>
{
    /** The initializer expression, if any
    */
    ConstantExprInfo<std::uint64_t> Initializer;

    //--------------------------------------------

    explicit EnumConstantSymbol(SymbolID ID) noexcept
        : SymbolCommonBase(ID)
    {
    }
};

MRDOCS_DECL
void
merge(EnumConstantSymbol& I, EnumConstantSymbol&& Other);

/** Map a EnumConstantSymbol to a dom::Object.

    @param t The tag type.
    @param io The IO object to use for mapping.
    @param I The EnumConstantSymbol to map.
    @param domCorpus The DomCorpus used to create
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    EnumConstantSymbol const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, I.asInfo(), domCorpus);
    if (!I.Initializer.Written.empty())
    {
        io.map("initializer", I.Initializer.Written);
    }
}

/** Map the EnumConstantSymbol to a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    EnumConstantSymbol const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_ENUMCONSTANT_HPP
