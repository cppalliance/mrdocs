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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_INLINE_TEXTINLINE_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_INLINE_TEXTINLINE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Inline/InlineBase.hpp>
#include <string>

namespace mrdocs::doc {

/** A Node containing a string of text.

    There will be no newlines in the text. Otherwise,
    this would be represented as multiple text nodes
    within a Paragraph node.
*/
struct TextInline
    : InlineCommonBase<InlineKind::Text>
{
    std::string literal;

    constexpr ~TextInline() override = default;

    constexpr TextInline() noexcept = default;

    explicit TextInline(std::string_view str) noexcept
        : literal(str)
    {}

    explicit TextInline(char const* str) noexcept
        : literal(str)
    {}

    explicit TextInline(std::string const& str) noexcept
        : literal(str)
    {}

    explicit TextInline(std::string&& str) noexcept
        : literal(std::move(str))
    {}

    auto operator<=>(TextInline const&) const = default;
    bool operator==(TextInline const&) const noexcept = default;
};

/** Map the @ref Text to a @ref dom::Object.

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
    TextInline const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Inline const&>(I), domCorpus);
    io.map("literal", I.literal);
}

/** Return the @ref Text as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    TextInline const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_TEXTINLINE_HPP
