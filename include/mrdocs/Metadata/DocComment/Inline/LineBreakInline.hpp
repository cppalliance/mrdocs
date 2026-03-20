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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_INLINE_LINEBREAKINLINE_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_INLINE_LINEBREAKINLINE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Inline/InlineBase.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** A hard line break that renders as "<br>"

    Syntax:

    @code
    first line\
    second line
    @endcode

    or

    @code
    first line<br>second line
    @endcode
*/
struct LineBreakInline
    : InlineCommonBase<InlineKind::LineBreak>
{
    /** Virtual destructor for the inline hierarchy.
    */
    constexpr ~LineBreakInline() override = default;

    /** Construct a line break node.
    */
    constexpr LineBreakInline() noexcept = default;

    /** Order line breaks (trivial).
    */
    auto operator<=>(LineBreakInline const&) const = default;

    /** Equality compares line breaks (trivial).
    */
    bool operator==(LineBreakInline const&) const noexcept = default;
};

MRDOCS_DESCRIBE_STRUCT(
    LineBreakInline,
    (Inline),
    ()
)

/** Map the @ref LineBreakInline to a @ref dom::Object.

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
    LineBreakInline const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Inline const&>(I), domCorpus);
}

/** Return the @ref LineBreakInline as a @ref dom::Value object.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    LineBreakInline const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_LINEBREAKINLINE_HPP
