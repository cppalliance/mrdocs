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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_THEMATICBREAKBLOCK_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_THEMATICBREAKBLOCK_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockBase.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParagraphBlock.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** A horizontal thematic break separating sections.

    Syntax:

    @code
    ---
    @endcode

    or

    @code
    ***
    @endcode

    or

    @code
    ___
    @endcode

    or

    @code
    - - -
    @endcode

    - You can use an asterisk (*), dash (-), or underscore (_).
    - There must be at least three characters in a row.
    - Spaces can be used between the characters, but no other characters.
    - The thematic break should be on its own line, separated by blank lines above and below.
    - Thematic breaks can also be used inside lists, but the character used for the break must be different from the list marker.

*/
struct ThematicBreakBlock final
    : BlockCommonBase<BlockKind::ThematicBreak>
{
    /** Copy constructor.
    */
    ThematicBreakBlock(ThematicBreakBlock const& other) = default;
    /** Copy assignment.
    */
    ThematicBreakBlock& operator=(ThematicBreakBlock const& other) = default;
    /** Compare break blocks (trivial as they hold no data).
    */
    auto operator<=>(ThematicBreakBlock const&) const = default;
};

MRDOCS_DESCRIBE_STRUCT(
    ThematicBreakBlock,
    (Block),
    ()
)

/** Map the @ref ThematicBreakBlock to a @ref dom::Object.

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
    ThematicBreakBlock const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Block const&>(I), domCorpus);
}

/** Return the @ref ThematicBreakBlock as a @ref dom::Value object.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    ThematicBreakBlock const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_THEMATICBREAKBLOCK_HPP
