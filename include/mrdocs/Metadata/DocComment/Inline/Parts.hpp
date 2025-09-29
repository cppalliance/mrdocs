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

/** Return the name of the Parts as a string.
 */
MRDOCS_DECL
dom::String
toString(Parts kind) noexcept;

/** Return the Parts from a @ref dom::Value string.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Parts const kind)
{
    v = toString(kind);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_PARTS_HPP
