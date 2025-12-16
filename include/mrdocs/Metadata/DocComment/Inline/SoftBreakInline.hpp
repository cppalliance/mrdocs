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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_INLINE_SOFTBREAKINLINE_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_INLINE_SOFTBREAKINLINE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Inline/InlineBase.hpp>
#include <string>

namespace mrdocs::doc {

/** A line break that may render as space
*/
struct SoftBreakInline
    : InlineCommonBase<InlineKind::SoftBreak>
{
    constexpr ~SoftBreakInline() override = default;
    constexpr SoftBreakInline() noexcept = default;
    auto operator<=>(SoftBreakInline const&) const = default;
    bool operator==(SoftBreakInline const&) const noexcept = default;
};

/** Map the @ref SoftBreak to a @ref dom::Object.

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
    SoftBreakInline const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Inline const&>(I), domCorpus);
}

/** Return the @ref SoftBreak as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    SoftBreakInline const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_SOFTBREAKINLINE_HPP
