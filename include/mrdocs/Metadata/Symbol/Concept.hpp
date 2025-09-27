//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SYMBOL_CONCEPT_HPP
#define MRDOCS_API_METADATA_SYMBOL_CONCEPT_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Metadata/Symbol.hpp>
#include <mrdocs/Metadata/Symbol/Source.hpp>
#include <mrdocs/Metadata/Template.hpp>

namespace mrdocs {

/** Info for concepts.
*/
struct ConceptSymbol final
    : SymbolCommonBase<SymbolKind::Concept>
{
    /** The concepts template parameters
    */
    Optional<TemplateInfo> Template;

    /** The concepts constraint-expression
    */
    ExprInfo Constraint;

    //--------------------------------------------

    explicit ConceptSymbol(SymbolID const &ID) noexcept
        : SymbolCommonBase(ID)
    {
    }

    std::strong_ordering
    operator<=>(ConceptSymbol const& other) const;
};

MRDOCS_DECL
void
merge(ConceptSymbol& I, ConceptSymbol&& Other);

/** Map a ConceptSymbol to a dom::Object.

    @param t The tag type.
    @param io The IO object to use for mapping.
    @param I The ConceptSymbol to map.
    @param domCorpus The DomCorpus used to create
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    ConceptSymbol const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, I.asInfo(), domCorpus);
    io.map("template", I.Template);
    if (!I.Constraint.Written.empty())
    {
        io.map("constraint", I.Constraint.Written);
    }
}

/** Map the ConceptSymbol to a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    ConceptSymbol const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_CONCEPT_HPP
