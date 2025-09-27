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

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Symbol/NamespaceAlias.hpp>
#include <llvm/ADT/STLExtras.h>

namespace mrdocs {

void
merge(NamespaceAliasSymbol& I, NamespaceAliasSymbol&& Other)
{
    MRDOCS_ASSERT(canMerge(I, Other));
    merge(I.asInfo(), std::move(Other.asInfo()));
    if (I.AliasedSymbol.Identifier.empty())
    {
        I.AliasedSymbol.Identifier = std::move(Other.AliasedSymbol.Identifier);
    }
    if (!I.AliasedSymbol.id)
    {
        I.AliasedSymbol.id = Other.AliasedSymbol.id;
    }
}

} // mrdocs

