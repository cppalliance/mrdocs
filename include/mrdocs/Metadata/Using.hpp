//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Fernando Pelliccioni (fpelliccioni@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_USING_HPP
#define MRDOCS_API_METADATA_USING_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Source.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <vector>
#include <utility>

namespace clang {
namespace mrdocs {

/** Info for using declarations and directives.
 */
struct UsingInfo
    : IsInfo<InfoKind::Using>,
    SourceInfo
{
    /** Indicates whether this is a using directive or a using declaration. */
    bool IsDirective = false;

    /** The symbols being "used". */
    std::vector<SymbolID> UsingSymbols;

    /** The qualifier for a using declaration/directive. */
    std::unique_ptr<NameInfo> Qualifier;

    //--------------------------------------------

    explicit UsingInfo(SymbolID ID) noexcept
        : IsInfo(ID)
    {
    }
};

} // mrdocs
} // clang

#endif
