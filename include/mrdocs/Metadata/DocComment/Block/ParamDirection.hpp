//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_PARAMDIRECTION_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_PARAMDIRECTION_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/Describe.hpp>

namespace mrdocs::doc {

/** Parameter pass direction.
*/
enum class ParamDirection
{
    /// No direction specified
    none,
    /// Parameter is passed
    in,
    /// Parameter is passed back to the caller
    out,
    /// Parameter is passed and passed back to the caller
    inout
};

MRDOCS_DESCRIBE_ENUM(
    ParamDirection,
    none, in, out, inout)
MRDOCS_DESCRIBE_ENUM_UNDEFINED(ParamDirection, none)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_PARAMDIRECTION_HPP
