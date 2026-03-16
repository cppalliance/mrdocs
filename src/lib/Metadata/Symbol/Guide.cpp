//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <lib/Support/Reflection/MergeReflectedType.hpp>
#include <lib/Support/Reflection/Reflection.hpp>
#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Symbol/Guide.hpp>
#include <mrdocs/Metadata/Symbol/Param.hpp>
#include <vector>

namespace mrdocs {

std::strong_ordering
GuideSymbol::
operator<=>(GuideSymbol const& other) const
{
    if (auto const cmp = Name <=> other.Name;
        !std::is_eq(cmp))
    {
        return cmp;
    }
    if (auto const cmp = Params.size() <=> other.Params.size();
        !std::is_eq(cmp))
    {
        return cmp;
    }
    if (auto const cmp = Template.operator bool() <=> other.Template.operator bool();
        !std::is_eq(cmp))
    {
        return cmp;
    }
    if (Template && other.Template)
    {
        if (auto const cmp = Template->Args.size() <=> other.Template->Args.size();
            !std::is_eq(cmp))
        {
            return cmp;
        }
        if (auto const cmp = Template->Params.size() <=> other.Template->Params.size();
            !std::is_eq(cmp))
        {
            return cmp;
        }
    }
    if (auto const cmp = Params.size() <=> other.Params.size();
        !std::is_eq(cmp))
    {
        return cmp;
    }
    for (size_t i = 0; i < Params.size(); ++i)
    {
        if (auto const cmp = Params[i] <=> other.Params[i];
            !std::is_eq(cmp))
        {
            return cmp;
        }
    }
    if (Template && other.Template)
    {
        if (auto const cmp = Template->Args <=> other.Template->Args;
            !std::is_eq(cmp))
        {
            return cmp;
        }
        if (auto const cmp = Template->Params <=> other.Template->Params;
            !std::is_eq(cmp))
        {
            return cmp;
        }
    }
    return this->asInfo() <=> other.asInfo();
}


void merge(GuideSymbol& I, GuideSymbol&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));
    mergeReflected(I, Other);
}

} // mrdocs
