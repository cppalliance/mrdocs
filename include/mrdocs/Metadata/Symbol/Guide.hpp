//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SYMBOL_GUIDE_HPP
#define MRDOCS_API_METADATA_SYMBOL_GUIDE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Symbol.hpp>
#include <mrdocs/Metadata/Symbol/Function.hpp>
#include <mrdocs/Metadata/Symbol/Source.hpp>
#include <mrdocs/Metadata/Template.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <vector>

namespace mrdocs {

/** Info for deduction guides.
*/
struct GuideSymbol final
    : SymbolCommonBase<SymbolKind::Guide>
{
    /** The pattern for the deduced specialization.

        This is always a SpecializationType.
    */
    Polymorphic<Type> Deduced = Polymorphic<Type>(AutoType{});

    /** Template head, if any.
    */
    Optional<TemplateInfo> Template;

    /** The parameters of the deduction guide.
    */
    std::vector<Param> Params;

    /** The explicit-specifier, if any.
    */
    ExplicitInfo Explicit;

    //--------------------------------------------

    explicit GuideSymbol(SymbolID ID) noexcept
        : SymbolCommonBase(ID)
    {}

    std::strong_ordering
    operator<=>(GuideSymbol const& other) const;
};

MRDOCS_DECL
void
merge(GuideSymbol& I, GuideSymbol&& Other);

/** Map a GuideSymbol to a dom::Object.

    @param t The tag type.
    @param io The IO object to use for mapping.
    @param I The GuideSymbol to map.
    @param domCorpus The DomCorpus used to create
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    GuideSymbol const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, I.asInfo(), domCorpus);
    io.map("params", dom::LazyArray(I.Params, domCorpus));
    io.map("deduced", I.Deduced);
    io.map("template", I.Template);
    io.map("explicitSpec", I.Explicit);
}

/** Map the GuideSymbol to a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    GuideSymbol const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_GUIDE_HPP
