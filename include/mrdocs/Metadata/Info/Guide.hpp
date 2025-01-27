//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_GUIDE_HPP
#define MRDOCS_API_METADATA_GUIDE_HPP

#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Source.hpp>
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
    PolymorphicValue<TypeInfo> Deduced;

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
    {
    }
};

} // clang::mrdocs

#endif
