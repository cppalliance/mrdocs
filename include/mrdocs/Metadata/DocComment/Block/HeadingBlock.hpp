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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_HEADINGBLOCK_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_HEADINGBLOCK_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockBase.hpp>
#include <string>

namespace mrdocs::doc {

/** A manually specified section heading.

    Syntax:

    @code
    @par Heading
    @endcode

    or in Markdown:

    @code
    # Heading 1
    ## Heading 2
    ### Heading 3
    #### Heading 4
    ##### Heading 5
    ###### Heading 6
    @endcode
*/
struct HeadingBlock final
    : BlockCommonBase<BlockKind::Heading>
    , InlineContainer
{
    /** Heading depth (1..6).
    */
    unsigned level = 1; // 1 to 6

    /** Inherit inline container constructors.
    */
    using InlineContainer::InlineContainer;

    /** Order headings by level and inline content.
    */
    auto operator<=>(HeadingBlock const&) const = default;

    /** Equality compares level and inline content.
    */
    bool operator==(HeadingBlock const&) const noexcept = default;
};

/** Map the @ref HeadingBlock to a @ref dom::Object.

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
    HeadingBlock const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Block const&>(I), domCorpus);
    tag_invoke(t, io, dynamic_cast<InlineContainer const&>(I), domCorpus);
    io.map("level", I.level);
}

/** Return the @ref HeadingBlock as a @ref dom::Value object.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    HeadingBlock const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_HEADINGBLOCK_HPP
