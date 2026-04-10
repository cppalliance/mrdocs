//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_DEFINITIONLISTITEM_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_DEFINITIONLISTITEM_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockBase.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParagraphBlock.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** An item in a definition list
*/
struct DefinitionListItem final
    : BlockContainer
{
    /** The term being defined.
    */
    InlineContainer term;

};

MRDOCS_DESCRIBE_STRUCT(
    DefinitionListItem,
    (BlockContainer),
    (term)
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_DEFINITIONLISTITEM_HPP
