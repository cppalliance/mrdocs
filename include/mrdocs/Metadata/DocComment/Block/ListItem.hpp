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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_LISTITEM_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_LISTITEM_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockBase.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParagraphBlock.hpp>
#include <string>

namespace mrdocs::doc {

/** An item in a list
*/
struct ListItem final
    : BlockContainer
{
    auto operator<=>(ListItem const&) const = default;
    bool operator==(ListItem const&) const noexcept = default;
};

/** Map the @ref ListItem to a @ref dom::Object.

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
    ListItem const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, static_cast<BlockContainer const&>(I), domCorpus);
}

/** Return the @ref ListItem as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    ListItem const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_LISTITEM_HPP
