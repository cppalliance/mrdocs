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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_INLINE_LINKINLINE_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_INLINE_LINKINLINE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Inline/TextInline.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** A hyperlink.

    Syntax:

    @code
    @link https://example.com label @endlink
    @endcode

    Or with markdown syntax:

    @code
    [link text](URL)
    [link text](URL "title")
    @endcode

    Or:

    @code
    <a href="https://www.example.com">Visit Example.com</a>
    @endcode
*/
struct LinkInline final
    : InlineCommonBase<InlineKind::Link>
    , InlineContainer
{
    /** Destination of the hyperlink.
    */
    std::string href;

    /** Construct an empty link.
    */
    LinkInline() = default;

    /** Construct a link with display text and target.

        @param text Link text to display.
        @param href Destination URI.
    */
    LinkInline(std::string_view text, std::string_view href)
        : InlineContainer(text)
        , href(href)
    {}

    /** Order links by href and child content.
    */
    auto operator<=>(LinkInline const&) const = default;
    /** Equality compares href and child content.
    */
    bool operator==(LinkInline const&) const noexcept = default;
};

MRDOCS_DESCRIBE_STRUCT(
    LinkInline,
    (Inline, InlineContainer),
    (href)
)

/** Map the @ref LinkInline to a @ref dom::Object.

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
    LinkInline const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Inline const&>(I), domCorpus);
    tag_invoke(t, io, dynamic_cast<InlineContainer const&>(I), domCorpus);
    io.map("href", I.href);
}

/** Return the @ref LinkInline as a @ref dom::Value object.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    LinkInline const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_LINKINLINE_HPP
