//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_INFO_GUIDE_HPP
#define MRDOCS_API_METADATA_INFO_GUIDE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Info/Function.hpp>
#include <mrdocs/Metadata/Info/Source.hpp>
#include <mrdocs/Metadata/Template.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <vector>

namespace clang::mrdocs {

/** Info for deduction guides.
*/
struct GuideInfo final
    : InfoCommonBase<InfoKind::Guide>
{
    /** The pattern for the deduced specialization.

        This is always a SpecializationTypeInfo.
    */
    Optional<Polymorphic<TypeInfo>> Deduced = std::nullopt;

    /** Template head, if any.
    */
    std::optional<TemplateInfo> Template;

    /** The parameters of the deduction guide.
    */
    std::vector<Param> Params;

    /** The explicit-specifier, if any.
    */
    ExplicitInfo Explicit;

    //--------------------------------------------

    explicit GuideInfo(SymbolID ID) noexcept
        : InfoCommonBase(ID)
    {}

    std::strong_ordering
    operator<=>(GuideInfo const& other) const;
};

MRDOCS_DECL
void
merge(GuideInfo& I, GuideInfo&& Other);

/** Map a GuideInfo to a dom::Object.

    @param t The tag type.
    @param io The IO object to use for mapping.
    @param I The GuideInfo to map.
    @param domCorpus The DomCorpus used to create
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    GuideInfo const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Info const&>(I), domCorpus);
    io.map("params", dom::LazyArray(I.Params, domCorpus));
    io.map("deduced", I.Deduced);
    io.map("template", I.Template);
    io.map("explicitSpec", I.Explicit);
}

/** Map the GuideInfo to a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    GuideInfo const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // clang::mrdocs

#endif // MRDOCS_API_METADATA_INFO_GUIDE_HPP
