//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/Dom/LazyObject.hpp>
#include "lib/Support/Radix.hpp"
#include <clang/AST/Type.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Metadata/Info/Record.hpp>

namespace clang::mrdocs {

dom::String
toString(InfoKind kind) noexcept
{
    switch(kind)
    {
#define INFO(PascalName, LowerName) \
    case InfoKind::PascalName: return #LowerName;
#include <mrdocs/Metadata/InfoNodesPascalAndLower.inc>
    default:
        MRDOCS_UNREACHABLE();
    }
}

void
merge(Info& I, Info&& Other)
{
    MRDOCS_ASSERT(I.id);
    merge(dynamic_cast<SourceInfo&>(I), std::move(dynamic_cast<SourceInfo&>(Other)));
    if (I.Name == "")
    {
        I.Name = Other.Name;
    }
    if (I.Parent)
    {
        I.Parent = Other.Parent;
    }
    if (I.Access == AccessKind::None)
    {
        I.Access = Other.Access;
    }
    I.Extraction = leastSpecific(I.Extraction, Other.Extraction);

    // Append javadocs
    if (!I.javadoc)
    {
        I.javadoc = std::move(Other.javadoc);
    }
    else if (Other.javadoc)
    {
        merge(*I.javadoc, std::move(*Other.javadoc));
    }
}

bool
operator<(
    Info const& lhs,
    Info const& rhs) noexcept
{
    // Consider kind
    if (lhs.Kind != rhs.Kind)
    {
        return lhs.Kind < rhs.Kind;
    }

    // Consider name
    if (lhs.Name != rhs.Name)
    {
        return lhs.Name < rhs.Name;
    }

    // Consider template arguments
    auto getTemplateInfoFn = [](auto const& U)
        -> TemplateInfo const*
    {
        if constexpr (requires { U.Template; })
        {
            MRDOCS_CHECK_OR(U.Template, nullptr);
            return &*U.Template;
        }
        return nullptr;
    };
    TemplateInfo const* lhsTemplate = visit(lhs, getTemplateInfoFn);
    TemplateInfo const* rhsTemplate = visit(rhs, getTemplateInfoFn);
    if (!lhsTemplate || !rhsTemplate) {
        return lhsTemplate != nullptr;
    }
    if (lhsTemplate->Args.size() != rhsTemplate->Args.size())
    {
        return lhsTemplate->Args.size() < rhsTemplate->Args.size();
    }
    for (std::size_t i = 0; i < lhsTemplate->Args.size(); ++i)
    {
        if (lhsTemplate->Args[i] != rhsTemplate->Args[i])
        {
            return lhsTemplate->Args[i]->Kind < rhsTemplate->Args[i]->Kind;
        }
    }
    return false;
}

} // clang::mrdocs
