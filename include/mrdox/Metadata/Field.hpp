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

#ifndef MRDOX_API_METADATA_FIELD_HPP
#define MRDOX_API_METADATA_FIELD_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Info.hpp>
#include <mrdox/Metadata/Source.hpp>
#include <mrdox/Metadata/Type.hpp>
#include <mrdox/ADT/BitField.hpp>
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
    : IsInfo<InfoKind::Field>
    , SourceInfo
{
    /** Type of the field */
    std::unique_ptr<TypeInfo> Type;

    /** The default member initializer, if any.
    */
    std::string Default;

    // attributes (nodiscard, no_unique_address, deprecated)
    FieldFlags specs;

    //--------------------------------------------

    explicit
    FieldInfo(
        SymbolID ID = SymbolID::zero) noexcept
        : IsInfo(ID)
    {
    }
};

} // mrdox
} // clang

#endif
