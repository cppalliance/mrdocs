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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_LISTBLOCK_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_LISTBLOCK_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockBase.hpp>
#include <mrdocs/Metadata/DocComment/Block/ListItem.hpp>
#include <mrdocs/Metadata/DocComment/Block/ListKind.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParagraphBlock.hpp>
#include <string>
#include <vector>

namespace mrdocs::doc {

/** A list of list items

    Syntax:

    @code
    - item one
    - item two
    @endcode

    Unordered list:

    @code
    - Item 1
    - Item 2
      - Nested item 2.1
      - Nested item 2.2
    * Item 3
    + Item 4
    @endcode

    Ordered list:

    @code
    1. First item
    2. Second item
        1. Nested ordered item 2.1
        2. Nested ordered item 2.2
    3. Third item
    @endcode

    Task lists (Checklists):

    @code
    - [x] Completed task
    - [ ] Pending task
    - [x] Another completed task
    @endcode

    Definition Lists (@ref DefinitionListBlock)

    @code
    Term 1
    : Definition of Term 1

    Term 2
    : Definition of Term 2
    : Another definition for Term 2
    @endcode
*/
struct ListBlock final
    : BlockCommonBase<BlockKind::List>
{
    /** Items contained in the list.
    */
    std::vector<ListItem> items;

    /** Display style for the list.
    */
    ListKind listKind = ListKind::Unordered;

    /** Order lists by item content and list style.
    */
    auto operator<=>(ListBlock const& other) const {
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

    /** Equality compares the list style and items.
    */
    bool
    operator==(ListBlock const&) const noexcept = default;
};

/** Map the @ref ListBlock to a @ref dom::Object.

    @param t The tag.
    @param io The output object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
*/
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    ListBlock const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Block const&>(I), domCorpus);
    io.defer("items", [&I, domCorpus] {
        return dom::LazyArray(I.items, domCorpus);
    });
    io.map("listKind", toString(I.listKind));
}

/** Return the @ref ListBlock as a @ref dom::Value object.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    ListBlock const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_LISTBLOCK_HPP
