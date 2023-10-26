//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_FIELD_HPP
#define MRDOCS_API_METADATA_FIELD_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Source.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <mrdocs/ADT/BitField.hpp>
#include <utility>

namespace clang {
namespace mrdocs {

union FieldFlags
{
    BitFieldFullValue raw{.value=0u};

    // KRYSTIAN FIXME: nodiscard cannot be applied to fields; this should
    // instead be isMaybeUnused. we should also store the spelling
    BitFlag<0> isMaybeUnused;
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

    // attributes (maybe_unused, no_unique_address, deprecated)
    FieldFlags specs;

    /** Whether the field is declared mutable */
    bool IsMutable = false;

    /** Whether the field is a bitfield */
    bool IsBitfield = false;

    /** The width of the bitfield */
    ConstantExprInfo<std::uint64_t> BitfieldWidth;

    //--------------------------------------------

    explicit
    FieldInfo(
        SymbolID ID = SymbolID::zero) noexcept
        : IsInfo(ID)
    {
    }
};

} // mrdocs
} // clang

#endif
