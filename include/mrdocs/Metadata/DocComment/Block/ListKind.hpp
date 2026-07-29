//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_LISTKIND_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_LISTKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>

namespace mrdocs {

/** Classification of list ordering.
*/
enum class ListKind {
    /// A bulleted list with no inherent ordering.
    Unordered,
    /// A numbered list where item order matters.
    Ordered
};

MRDOCS_DESCRIBE_ENUM(ListKind, Unordered, Ordered)

} // mrdocs

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_LISTKIND_HPP
