//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
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
#include <mrdocs/Support/Reflection/Describe.hpp>

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

    /** Construct a concept symbol with its ID.
    */
    explicit ConceptSymbol(SymbolID const &ID) noexcept
        : SymbolCommonBase(ID)
    {
    }

    /** Compare concept symbols by base info, template, and constraint.
    */
    std::strong_ordering
    operator<=>(ConceptSymbol const& other) const;
};

MRDOCS_DESCRIBE_STRUCT(
    ConceptSymbol,
    (SymbolCommonBase<SymbolKind::Concept>),
    (Template, Constraint)
)

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_CONCEPT_HPP
