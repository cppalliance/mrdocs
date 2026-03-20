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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_PRECONDITIONBLOCK_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_PRECONDITIONBLOCK_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParagraphBlock.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** Text describing required conditions before entry.

    Syntax:

    @code
    @pre condition
    @endcode
*/
struct PreconditionBlock final
    : BlockCommonBase<BlockKind::Precondition>
    , InlineContainer
{
    /** Inherit inline container constructors.
    */
    using InlineContainer::InlineContainer;

    /** Order preconditions by their inline text.
    */
    auto operator<=>(PreconditionBlock const&) const = default;

    /** Equality compares the inline text.
    */
    bool operator==(PreconditionBlock const&) const noexcept = default;
};

MRDOCS_DESCRIBE_STRUCT(
    PreconditionBlock,
    (Block, InlineContainer),
    ()
)

/** Map the @ref PreconditionBlock to a @ref dom::Object.

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
    PreconditionBlock const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Block const&>(I), domCorpus);
    tag_invoke(t, io, dynamic_cast<InlineContainer const&>(I), domCorpus);
}

/** Return the @ref PreconditionBlock as a @ref dom::Value object.

    @param v The value to assign to.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    PreconditionBlock const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_PRECONDITIONBLOCK_HPP
