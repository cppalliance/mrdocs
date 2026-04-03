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
#include <mrdocs/Support/Describe.hpp>
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

    /** Construct a deduction guide symbol with its ID.
    */
    explicit GuideSymbol(SymbolID ID) noexcept
        : SymbolCommonBase(ID)
    {}

    /** Compare guides by params/deduced/template/explicit.
    */
    std::strong_ordering
    operator<=>(GuideSymbol const& other) const;
};

MRDOCS_DESCRIBE_STRUCT(
    GuideSymbol,
    (SymbolCommonBase<SymbolKind::Guide>),
    (Deduced, Template, Params, Explicit)
)

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_GUIDE_HPP
