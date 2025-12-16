//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TPARAM_CONSTANTTPARAM_HPP
#define MRDOCS_API_METADATA_TPARAM_CONSTANTTPARAM_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/TParam/TParamBase.hpp>
#include <mrdocs/Metadata/Type/TypeBase.hpp>

namespace mrdocs {

/** A constant template parameter

    Before C++26, constant template parameters were called
    non-type template parameter in the standard wording.
    The terminology was changed by P2841R6 / PR#7587.
 */
struct ConstantTParam final
    : TParamCommonBase<TParamKind::Constant>
{
    /** Type of the non-type template parameter */
    Polymorphic<struct Type> Type = Polymorphic<struct Type>(AutoType{});

    std::strong_ordering operator<=>(ConstantTParam const&) const;
};

} // mrdocs

#endif // MRDOCS_API_METADATA_TPARAM_CONSTANTTPARAM_HPP
