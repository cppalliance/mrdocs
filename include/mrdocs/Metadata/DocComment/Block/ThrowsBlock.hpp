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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_THROWSBLOCK_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_THROWSBLOCK_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParagraphBlock.hpp>
#include <mrdocs/Metadata/DocComment/Inline.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** Documentation for a function exception clause

    Syntax:

    @code
    @throws Type description
    @endcode
*/
struct ThrowsBlock final
    : BlockCommonBase<BlockKind::Throws>
    , InlineContainer
{
    /** Exception type being described.
    */
    ReferenceInline exception;

    /** Inherit inline container constructors.
    */
    using InlineContainer::InlineContainer;

};

MRDOCS_DESCRIBE_STRUCT(
    ThrowsBlock,
    (BlockCommonBase<BlockKind::Throws>, InlineContainer),
    (exception)
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_THROWSBLOCK_HPP
