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
};

} // clang::mrdocs

#endif
