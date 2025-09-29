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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_CODEBLOCK_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_CODEBLOCK_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockBase.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParagraphBlock.hpp>
#include <string>

namespace mrdocs::doc {

/** Preformatted source code.
 */
struct CodeBlock final
    : BlockCommonBase<BlockKind::Code>
{
    std::string literal;

    /// Fence info string, e.g. "cpp"
    std::string info;

    CodeBlock() noexcept = default;
    auto operator<=>(CodeBlock const&) const = default;
    bool operator==(CodeBlock const&) const noexcept = default;
};

/** Map the @ref Code to a @ref dom::Object.

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
    CodeBlock const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Block const&>(I), domCorpus);
    io.map("literal", I.literal);
    if (!I.info.empty())
    {
        io.map("info", I.info);
    }
}

/** Return the @ref Code as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    CodeBlock const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_CODEBLOCK_HPP
