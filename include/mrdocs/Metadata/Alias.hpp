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

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Source.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <utility>

namespace clang {
namespace mrdocs {

/** Info for namespace aliases.
*/
struct AliasInfo
    : IsInfo<InfoKind::Alias>
    , SourceInfo
{
    /** Aliased symbol. */
    SymbolID AliasedSymbol = SymbolID::invalid;

    /** The fully qualified name of the alias. */
    std::unique_ptr<NameInfo> FullyQualifiedName;

    //--------------------------------------------

    explicit AliasInfo(SymbolID ID) noexcept
        : IsInfo(ID)
    {
    }
};

} // mrdocs
} // clang

#endif
