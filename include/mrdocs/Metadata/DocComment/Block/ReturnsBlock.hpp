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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_RETURNSBLOCK_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_RETURNSBLOCK_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockBase.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParagraphBlock.hpp>
#include <string>

namespace mrdocs::doc {

/** Documentation for a function return type
*/
struct ReturnsBlock final
    : BlockCommonBase<BlockKind::Returns>
    , InlineContainer
{
    using InlineContainer::InlineContainer;
    ReturnsBlock(ReturnsBlock const&) = default;
    ReturnsBlock(ReturnsBlock&&) noexcept = default;
    ReturnsBlock(InlineContainer const& other) : InlineContainer(other) {}
    ReturnsBlock(InlineContainer&& other) noexcept : InlineContainer(other) {}
    ~ReturnsBlock() override = default;
    ReturnsBlock& operator=(ReturnsBlock const&) = default;
    ReturnsBlock& operator=(ReturnsBlock&&) noexcept = default;
    auto operator<=>(ReturnsBlock const&) const = default;
    bool operator==(ReturnsBlock const&) const noexcept = default;
};

/** Map the @ref Returns to a @ref dom::Object.

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
    ReturnsBlock const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Block const&>(I), domCorpus);
    tag_invoke(t, io, dynamic_cast<InlineContainer const&>(I), domCorpus);
}

/** Return the @ref Returns as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    ReturnsBlock const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_RETURNSBLOCK_HPP
