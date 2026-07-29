//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SYMBOL_ENUMCONSTANT_HPP
#define MRDOCS_API_METADATA_SYMBOL_ENUMCONSTANT_HPP

#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Metadata/Symbol.hpp>
#include <mrdocs/Metadata/Symbol/Source.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>

namespace mrdocs {

/** Info for enum constants.
*/
struct EnumConstantSymbol final
    : SymbolCommonBase<SymbolKind::EnumConstant>
{
    /** The initializer expression, if any
    */
    ConstantExprInfo<std::uint64_t> Initializer;

    //--------------------------------------------

    /** Construct an enum constant with its ID.
    */
    explicit EnumConstantSymbol(SymbolID ID) noexcept
        : SymbolCommonBase(ID)
    {
    }
};

MRDOCS_DESCRIBE_STRUCT(
    EnumConstantSymbol,
    (SymbolCommonBase<SymbolKind::EnumConstant>),
    (Initializer)
)

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_ENUMCONSTANT_HPP
