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

#include <mrdocs/Metadata/Info/Guide.hpp>
#include <mrdocs/Platform.hpp>
#include <llvm/ADT/STLExtras.h>

namespace clang::mrdocs {

void merge(GuideInfo& I, GuideInfo&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));
    merge(dynamic_cast<Info&>(I), std::move(dynamic_cast<Info&>(Other)));
    if (!I.Deduced)
    {
        I.Deduced = std::move(Other.Deduced);
    }
    if (I.Params.empty())
    {
        I.Params = std::move(Other.Params);
    }
    if (!I.Template)
    {
        I.Template = std::move(Other.Template);
    }
    if (I.Explicit.Implicit)
    {
        I.Explicit = std::move(Other.Explicit);
    }
}

} // clang::mrdocs

