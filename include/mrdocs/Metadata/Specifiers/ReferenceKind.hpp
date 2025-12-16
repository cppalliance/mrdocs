//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SPECIFIERS_REFERENCEKIND_HPP
#define MRDOCS_API_METADATA_SPECIFIERS_REFERENCEKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <string>

namespace mrdocs {

/** Reference type kinds
*/
enum class ReferenceKind
{
    /// Not a reference
    None = 0,
    /// An L-Value reference
    LValue,
    /// An R-Value reference
    RValue
};

MRDOCS_DECL dom::String toString(ReferenceKind kind) noexcept;

/** Return the ReferenceKind as a @ref dom::Value string.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    ReferenceKind kind)
{
    v = toString(kind);
}

} // mrdocs

#endif
