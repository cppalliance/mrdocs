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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_BRIEFBLOCK_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_BRIEFBLOCK_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockBase.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParagraphBlock.hpp>
#include <string>

namespace mrdocs::doc {

/** The brief description
*/
struct BriefBlock final
    : BlockCommonBase<BlockKind::Brief>
    , InlineContainer
{
    std::vector<std::string> copiedFrom;

    BriefBlock() = default;

    BriefBlock(BriefBlock const& other) = default;

    using InlineContainer::InlineContainer;

    BriefBlock&
    operator=(BriefBlock const& other) = default;

    using InlineContainer::operator=;

    auto operator<=>(BriefBlock const&) const = default;
};

/** Map the @ref Brief to a @ref dom::Object.

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
    BriefBlock const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Block const&>(I), domCorpus);
    tag_invoke(t, io, dynamic_cast<InlineContainer const&>(I), domCorpus);
    io.defer("copiedFrom", [&I] {
        return dom::LazyArray(I.copiedFrom);
    });
}

/** Return the @ref Brief as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    BriefBlock const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);

    Optional<BriefBlock> o;
}

inline
void
tag_invoke(
    mrdocs::dom::ValueFromTag,
    mrdocs::dom::Value& v,
    Optional<BriefBlock> const& I,
    DomCorpus const* domCorpus)
{
    if (!I)
    {
        v = nullptr;
        return;
    }
    tag_invoke(mrdocs::dom::ValueFromTag{}, v, *I, domCorpus);
}

static_assert(dom::HasValueFrom<BriefBlock, DomCorpus const*>);
static_assert(dom::HasValueFromWithContext<BriefBlock, DomCorpus const*>);

static_assert(dom::HasValueFrom<Optional<BriefBlock>, DomCorpus const*>);
static_assert(dom::HasValueFromWithContext<Optional<BriefBlock>, DomCorpus const*>);

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_BRIEFBLOCK_HPP
