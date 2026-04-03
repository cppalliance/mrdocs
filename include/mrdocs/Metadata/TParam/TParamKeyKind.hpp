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

#ifndef MRDOCS_API_METADATA_TPARAM_TPARAMKEYKIND_HPP
#define MRDOCS_API_METADATA_TPARAM_TPARAMKEYKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string_view>

namespace mrdocs {

/** The keyword a template parameter was declared with
*/
enum class TParamKeyKind : int
{
    /// Class keyword
    Class = 0,
    /// Typename keyword
    Typename
};

MRDOCS_DESCRIBE_ENUM(TParamKeyKind, Class, Typename)

} // mrdocs

#endif
