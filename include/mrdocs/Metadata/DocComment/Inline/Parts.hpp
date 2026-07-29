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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_INLINE_PARTS_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_INLINE_PARTS_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>

namespace mrdocs::doc {

/** Which parts of the documentation to copy.

    @li `all`: copy the brief and the description.
    @li `brief`: only copy the brief.
    @li `description`: only copy the description.
*/
enum class Parts
{
    /// Copy the brief and the description
    all = 1, // needed by bitstream
    /// Copy the brief
    brief,
    /// Copy the description
    description
};

MRDOCS_DESCRIBE_ENUM(
    Parts,
    all, brief, description)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_PARTS_HPP
