//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_ENUMCONSTANT_HPP
#define MRDOCS_API_METADATA_ENUMCONSTANT_HPP

#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Source.hpp>

namespace clang::mrdocs {

/** Info for enum constants.
*/
struct EnumConstantInfo final
    : InfoCommonBase<InfoKind::EnumConstant>
{
    /** The initializer expression, if any
    */
    ConstantExprInfo<std::uint64_t> Initializer;

    //--------------------------------------------

    explicit EnumConstantInfo(SymbolID ID) noexcept
        : InfoCommonBase(ID)
    {
    }
};

MRDOCS_DECL
void
merge(EnumConstantInfo& I, EnumConstantInfo&& Other);

/** Map a EnumConstantInfo to a dom::Object.

    @param t The tag type.
    @param io The IO object to use for mapping.
    @param I The EnumConstantInfo to map.
    @param domCorpus The DomCorpus used to create
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    EnumConstantInfo const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Info const&>(I), domCorpus);
    if (!I.Initializer.Written.empty())
    {
        io.map("initializer", I.Initializer.Written);
    }
}

/** Map the EnumConstantInfo to a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    EnumConstantInfo const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // clang::mrdocs

#endif
