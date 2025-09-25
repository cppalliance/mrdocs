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

#include <lib/Support/Radix.hpp>
#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/Dom/LazyObject.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Metadata/Info/Record.hpp>
#include <mrdocs/Support/String.hpp>
#include <clang/AST/Type.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

namespace clang::mrdocs {

dom::String
toString(InfoKind const kind) noexcept
{
    switch(kind)
    {
#define INFO(Type) \
    case InfoKind::Type: return toKebabCase(#Type);
#include <mrdocs/Metadata/Info/InfoNodes.inc>
    default:
        MRDOCS_UNREACHABLE();
    }
}

void
merge(Info& I, Info&& Other)
{
    MRDOCS_ASSERT(I.id);
    merge(I.Loc, std::move(Other.Loc));
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

} // clang::mrdocs
