//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_ENUMCONSTANT_HPP
#define MRDOCS_API_METADATA_ENUMCONSTANT_HPP

#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Source.hpp>

namespace clang::mrdocs {

/** Info for enum constants.
*/
struct EnumConstantInfo final
    : InfoCommonBase<InfoKind::EnumConstant>
{
    /** The initializer expression, if any
    */
    ConstantExprInfo<std::uint64_t> Initializer;

    //--------------------------------------------

    explicit EnumConstantInfo(SymbolID ID) noexcept
        : InfoCommonBase(ID)
    {
    }
};

} // clang::mrdocs

#endif
