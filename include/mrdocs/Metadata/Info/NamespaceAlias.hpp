//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Fernando Pelliccioni (fpelliccioni@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_NAMESPACEALIAS_HPP
#define MRDOCS_API_METADATA_NAMESPACEALIAS_HPP

#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Source.hpp>

namespace clang::mrdocs {

/** Info for namespace aliases.
*/
struct NamespaceAliasInfo final
    : InfoCommonBase<InfoKind::NamespaceAlias>
{
    /** The aliased symbol. */
    PolymorphicValue<NameInfo> AliasedSymbol;

    //--------------------------------------------

    explicit NamespaceAliasInfo(SymbolID const &ID) noexcept
        : InfoCommonBase(ID)
    {
    }
};

} // clang::mrdocs

#endif
