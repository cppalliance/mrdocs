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

#include <mrdocs/Metadata/Info/Typedef.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Platform.hpp>
#include <llvm/ADT/STLExtras.h>

namespace clang::mrdocs {

std::strong_ordering
TypedefInfo::
operator<=>(TypedefInfo const& other) const
{
    if (auto const cmp = Name <=> other.Name;
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
    return dynamic_cast<Info const&>(*this) <=> dynamic_cast<Info const&>(other);
}

void merge(TypedefInfo& I, TypedefInfo&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));
    merge(dynamic_cast<Info&>(I), std::move(dynamic_cast<Info&>(Other)));
    if (!I.IsUsing)
        I.IsUsing = Other.IsUsing;
    if (! I.Type)
        I.Type = std::move(Other.Type);
    if(! I.Template)
        I.Template = std::move(Other.Template);
}

} // clang::mrdocs

