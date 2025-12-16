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

/** Return the name of the ParamDirection as a string.
 */
MRDOCS_DECL
dom::String
toString(ParamDirection kind) noexcept;

/** Return the ParamDirection from a @ref dom::Value string.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    ParamDirection const kind)
{
    v = toString(kind);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_PARAMDIRECTION_HPP
