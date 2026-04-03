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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_DEFINITIONLISTBLOCK_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_DEFINITIONLISTBLOCK_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockBase.hpp>
#include <mrdocs/Metadata/DocComment/Block/DefinitionListItem.hpp>
#include <mrdocs/Metadata/DocComment/Block/ListKind.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParagraphBlock.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>
#include <vector>

namespace mrdocs::doc {

/** A list of terms paired with definitions.

    Syntax:

    @code
    Markdown
    : A lightweight markup language
    : Created by John Gruber in 2004

    HTML
    : A standard markup language used to create web pages
    : Used for structuring content on the internet
    @endcode

    In HTML, it looks like:

    @code
    <dl>
        <dt>First Term</dt>
        <dd>This is the definition of the first term.</dd>
        <dt>Second Term</dt>
        <dd>This is one definition of the second term. </dd>
        <dd>This is another definition of the second term.</dd>
    </dl>
    @endcode
*/
struct DefinitionListBlock final
    : BlockCommonBase<BlockKind::List>
{
    /** Sequence of definition list items.
    */
    std::vector<DefinitionListItem> items;

    /** Order items and their definitions lexicographically.
    */
    auto operator<=>(DefinitionListBlock const& other) const {
        if (auto const cmp = items.size() <=> other.items.size();
            !std::is_eq(cmp))
        {
            return cmp;
        }
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            if (auto const cmp = items[i] <=> other.items[i];
                !std::is_eq(cmp))
            {
                return cmp;
            }
        }
        return std::strong_ordering::equal;
    }

    /** Equality compares the contained items.
    */
    bool
    operator==(DefinitionListBlock const&) const noexcept = default;
};

MRDOCS_DESCRIBE_STRUCT(
    DefinitionListBlock,
    (BlockCommonBase<BlockKind::List>),
    (items)
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_DEFINITIONLISTBLOCK_HPP
