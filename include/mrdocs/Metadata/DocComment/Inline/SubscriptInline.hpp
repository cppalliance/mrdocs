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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_INLINE_SUBSCRIPTINLINE_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_INLINE_SUBSCRIPTINLINE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/DocComment/Inline/TextInline.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** Subscript text fragment (lowered baseline).

    Syntax:

    @code
    H~2~O
    @endcode
*/
struct SubscriptInline final
    : InlineCommonBase<InlineKind::Subscript>
    , InlineContainer
{
    /** Order subscript spans by their children.
    */
    auto operator<=>(SubscriptInline const&) const = default;
    /** Equality compares contained inline children.
    */
    bool operator==(SubscriptInline const&) const noexcept = default;
};

MRDOCS_DESCRIBE_STRUCT(
    SubscriptInline,
    (Inline, InlineContainer),
    ()
)

/** Map the @ref SubscriptInline to a @ref dom::Object.

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
    SubscriptInline const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Inline const&>(I), domCorpus);
    tag_invoke(t, io, dynamic_cast<InlineContainer const&>(I), domCorpus);
}

/** Return the @ref SubscriptInline as a @ref dom::Value object.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    SubscriptInline const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_SUBSCRIPTINLINE_HPP
