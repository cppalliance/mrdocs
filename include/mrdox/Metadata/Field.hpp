//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_METADATA_FIELD_HPP
#define MRDOX_METADATA_FIELD_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Info.hpp>
#include <mrdox/Metadata/Symbol.hpp>
#include <mrdox/Metadata/Type.hpp>
#include <mrdox/ADT/BitField.hpp>
#include <llvm/ADT/SmallString.h>
#include <utility>

namespace clang {
namespace mrdox {

union FieldFlags
{
    BitFieldFullValue raw{.value=0u};

    // KRYSTIAN FIXME: nodiscard cannot be applied to fields; this should
    // instead be isMaybeUnused. we should also store the spelling
    BitFlag<0> isNodiscard;
    BitFlag<1> isDeprecated;
    BitFlag<2> hasNoUniqueAddress;
};

/** Info for fields (i.e. non-static data members)

    Non-static data members cannot be redeclared.
*/
struct FieldInfo
    : SymbolInfo
{
    /** Type of the field */
    TypeInfo Type;

    // std::string Name; // Name associated with this info.

    // When used for function parameters or variables,
    // contains the string representing the expression of the default value,
    // or the variable initializer if any.
    std::string Default;

    // attributes (nodiscard, no_unique_address, deprecated)
    FieldFlags specs;

    //--------------------------------------------

    static constexpr InfoType type_id = InfoType::IT_field;

    FieldInfo(
        SymbolID ID = SymbolID::zero,
        llvm::StringRef Name = llvm::StringRef())
        : SymbolInfo(InfoType::IT_field, ID, Name)
    {
    }
};

} // mrdox
} // clang

#endif
