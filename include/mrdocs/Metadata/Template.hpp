//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TEMPLATE_HPP
#define MRDOCS_API_METADATA_TEMPLATE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Metadata/Symbol/SymbolID.hpp>
#include <mrdocs/Metadata/TArg.hpp>
#include <mrdocs/Metadata/TParam.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <vector>

namespace mrdocs {

/** The kind of template or specialization.
*/
enum class TemplateSpecKind
{
    /// Primary template
    Primary,
    /// Full template specialization
    Explicit,
    /// Partial template specialization
    Partial
};

MRDOCS_DESCRIBE_ENUM(
    TemplateSpecKind,
    Primary, Explicit, Partial)


/** Information about templates and specializations thereof.
*/
struct TemplateInfo final
{
    /** Template parameter list.
    */
    std::vector<Polymorphic<TParam>> Params;

    /** Bound arguments for specializations.
    */
    std::vector<Polymorphic<TArg>> Args;

    /** The requires-clause for the template parameter list, if any.
    */
    ExprInfo Requires;

    /** Primary template ID for partial and explicit specializations.
    */
    SymbolID Primary = SymbolID::invalid;

    // KRYSTIAN NOTE: using the presence of args/params
    // to determine the specialization kind *should* work.
    // emphasis on should.
    TemplateSpecKind

    /** Deduce which specialization category this info represents.
    */
    specializationKind() const noexcept
    {
        if (Params.empty())
        {
            return TemplateSpecKind::Explicit;
        }
        if (Args.empty())
        {
            return TemplateSpecKind::Primary;
        }
        return TemplateSpecKind::Partial;
    }

};

MRDOCS_DESCRIBE_STRUCT(TemplateInfo, (), (Params, Args, Requires, Primary))


/** Merge partial template info, filling missing pieces.
*/
MRDOCS_DECL
void
merge(TemplateInfo& I, TemplateInfo&& Other);


} // mrdocs

#endif
