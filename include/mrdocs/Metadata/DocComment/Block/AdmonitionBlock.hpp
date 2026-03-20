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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_ADMONITIONBLOCK_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_ADMONITIONBLOCK_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Block/AdmonitionKind.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockBase.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParagraphBlock.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** A block for side-notes like tips, warnings, notes

    This paragraph represents an admonition, such as
    a note, tip, important, caution, or warning.


    Syntax:

    @code
    @note text
    @endcode
*/
struct AdmonitionBlock final
    : BlockCommonBase<BlockKind::Admonition>
    , BlockContainer
{
    /** The kind of admonition

        This is typically a string in other implementations.
    */
    AdmonitionKind admonish;

    /// Optional title for the admonition
    Optional<Polymorphic<Inline>> Title;

    /** Construct an admonition with the given kind.
    */
    explicit
    AdmonitionBlock(
        AdmonitionKind const admonish_ = AdmonitionKind::none) noexcept
        : admonish(admonish_)
    {}

    /** Compare admonitions by kind, title, and contents.
    */
    auto operator<=>(AdmonitionBlock const&) const = default;

    /** Equality compares the admonition contents.
    */
    bool operator==(AdmonitionBlock const&) const noexcept = default;
};

MRDOCS_DESCRIBE_STRUCT(
    AdmonitionBlock,
    (Block, BlockContainer),
    (admonish)
)

/** Map the @ref AdmonitionBlock to a @ref dom::Object.

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
    AdmonitionBlock const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Block const&>(I), domCorpus);
    tag_invoke(t, io, dynamic_cast<BlockContainer const&>(I), domCorpus);
    io.map("admonish", I.admonish);
}

/** Return the @ref AdmonitionBlock as a @ref dom::Value object.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    AdmonitionBlock const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_ADMONITIONBLOCK_HPP
