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

#include <mrdocs/Metadata/Info/Variable.hpp>
#include <mrdocs/Platform.hpp>
#include <llvm/ADT/STLExtras.h>

namespace clang::mrdocs {

std::strong_ordering
VariableInfo::
operator<=>(VariableInfo const& other) const
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
    return this->asInfo() <=> other.asInfo();
}

void
merge(VariableInfo& I, VariableInfo&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));
    merge(I.asInfo(), std::move(Other.asInfo()));
    if (!I.Type)
    {
        I.Type = std::move(Other.Type);
    }
    if (!I.Template)
    {
        I.Template = std::move(Other.Template);
    }
    if (I.Initializer.Written.empty())
    {
        I.Initializer = std::move(Other.Initializer);
    }
    I.IsConstinit |= Other.IsConstinit;
    I.IsThreadLocal |= Other.IsThreadLocal;
    I.IsConstexpr |= Other.IsConstexpr;
    I.IsInline |= Other.IsInline;
    if (I.StorageClass == StorageClassKind::None)
    {
        I.StorageClass = Other.StorageClass;
    }
    for (auto& otherAttr: Other.Attributes)
    {
        if (std::ranges::find(I.Attributes, otherAttr) == I.Attributes.end())
        {
            I.Attributes.push_back(otherAttr);
        }
    }
}

} // clang::mrdocs

