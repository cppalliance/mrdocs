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

    /** Order throw clauses by exception and description.
    */
    auto operator<=>(ThrowsBlock const&) const = default;

    /** Equality compares exception and description.
    */
    bool operator==(ThrowsBlock const&) const noexcept = default;
};

/** Map the @ref ThrowsBlock to a @ref dom::Object.

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
    ThrowsBlock const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Block const&>(I), domCorpus);
    tag_invoke(t, io, dynamic_cast<InlineContainer const&>(I), domCorpus);
    io.map("exception", I.exception);
}

/** Return the @ref ThrowsBlock as a @ref dom::Value object.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    ThrowsBlock const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_THROWSBLOCK_HPP
