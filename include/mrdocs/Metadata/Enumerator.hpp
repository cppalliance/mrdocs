//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_ENUMERATOR_HPP
#define MRDOCS_API_METADATA_ENUMERATOR_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Source.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <utility>

namespace clang {
namespace mrdocs {

/** Info for enumerators.
*/
struct EnumeratorInfo
    : IsInfo<InfoKind::Enumerator>
    , SourceInfo
{
    /** The initializer expression, if any
    */
    ConstantExprInfo<std::uint64_t> Initializer;

    //--------------------------------------------

    explicit EnumeratorInfo(SymbolID ID) noexcept
        : IsInfo(ID)
    {
    }
};

} // mrdocs
} // clang

#endif
