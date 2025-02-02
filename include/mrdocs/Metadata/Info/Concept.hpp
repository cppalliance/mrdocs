//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_CONCEPT_HPP
#define MRDOCS_API_METADATA_CONCEPT_HPP

#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Metadata/Source.hpp>
#include <mrdocs/Metadata/Template.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>

namespace clang::mrdocs {

/** Info for concepts.
*/
struct ConceptInfo final
    : InfoCommonBase<InfoKind::Concept>
{
    /** The concepts template parameters
    */
    std::optional<TemplateInfo> Template;

    /** The concepts constraint-expression
    */
    ExprInfo Constraint;

    //--------------------------------------------

    explicit ConceptInfo(SymbolID const &ID) noexcept
        : InfoCommonBase(ID)
    {
    }

    std::strong_ordering
    operator<=>(ConceptInfo const& other) const;
};

MRDOCS_DECL
void
merge(ConceptInfo& I, ConceptInfo&& Other);

/** Map a ConceptInfo to a dom::Object.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    ConceptInfo const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Info const&>(I), domCorpus);
    io.map("template", I.Template);
    if (!I.Constraint.Written.empty())
    {
        io.map("constraint", I.Constraint.Written);
    }
}

/** Map the ConceptInfo to a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    ConceptInfo const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // clang::mrdocs

#endif
