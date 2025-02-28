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

#include <mrdocs/Metadata/Info/Field.hpp>
#include <mrdocs/Platform.hpp>
#include <llvm/ADT/STLExtras.h>

namespace clang::mrdocs {

void
merge(FieldInfo& I, FieldInfo&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));
    merge(dynamic_cast<Info&>(I), std::move(dynamic_cast<Info&>(Other)));
    if (!I.Type)
    {
        I.Type = std::move(Other.Type);
    }
    if (I.Default.Written.empty())
    {
        I.Default = std::move(Other.Default);
    }
    I.IsBitfield |= Other.IsBitfield;
    merge(I.BitfieldWidth, std::move(Other.BitfieldWidth));
    I.IsVariant |= Other.IsVariant;
    I.IsMutable |= Other.IsMutable;
    I.IsMaybeUnused |= Other.IsMaybeUnused;
    I.IsDeprecated |= Other.IsDeprecated;
    I.HasNoUniqueAddress |= Other.HasNoUniqueAddress;
}

} // clang::mrdocs

