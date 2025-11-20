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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_INLINE_STRIKETHROUGHINLINE_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_INLINE_STRIKETHROUGHINLINE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/DocComment/Inline/TextInline.hpp>
#include <string>

namespace mrdocs::doc {

/** Strikethrough span to show removed or deprecated text.

    Syntax:

    @code
    ~~crossed out~~
    @endcode

    When rendered to HTML, the Markdown syntax above
    typically translates into the `del` tag, which
    represents content that has been deleted or is no
    longer accurate.

    @code
    <del>This text is struck through.</del>
    @endcode

*/
struct StrikethroughInline final
    : InlineCommonBase<InlineKind::Strikethrough>
    , InlineContainer
{
    /** Order strikethrough spans by their children.
    */
    auto operator<=>(StrikethroughInline const&) const = default;
    /** Equality compares contained inline children.
    */
    bool operator==(StrikethroughInline const&) const noexcept = default;
};

/** Map the @ref StrikethroughInline to a @ref dom::Object.

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
    StrikethroughInline const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Inline const&>(I), domCorpus);
    tag_invoke(t, io, dynamic_cast<InlineContainer const&>(I), domCorpus);
}

/** Return the @ref StrikethroughInline as a @ref dom::Value object.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    StrikethroughInline const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_STRIKETHROUGHINLINE_HPP
