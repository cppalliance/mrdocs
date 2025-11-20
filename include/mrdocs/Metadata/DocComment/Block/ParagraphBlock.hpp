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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_PARAGRAPHBLOCK_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_PARAGRAPHBLOCK_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockBase.hpp>
#include <mrdocs/Metadata/DocComment/Inline.hpp>
#include <string>

namespace mrdocs::doc {

/** A sequence of text nodes.

    Syntax:

    @code
    Plain paragraph text.

    Another paragraph.
    @endcode
*/
struct ParagraphBlock
    : BlockCommonBase<BlockKind::Paragraph>
    , InlineContainer
{
    /** Virtual destructor for the polymorphic block hierarchy.
    */
    ~ParagraphBlock() override = default;

    /** Construct an empty paragraph.
    */
    ParagraphBlock() noexcept = default;

    /** Compare paragraphs by their inline content.
    */
    auto operator<=>(ParagraphBlock const&) const = default;
};

/** Map the @ref ParagraphBlock to a @ref dom::Object.

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
    ParagraphBlock const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Block const&>(I), domCorpus);
    tag_invoke(t, io, static_cast<InlineContainer const&>(I), domCorpus);
}

/** Return the @ref ParagraphBlock as a @ref dom::Value object.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    ParagraphBlock const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_PARAGRAPHBLOCK_HPP
