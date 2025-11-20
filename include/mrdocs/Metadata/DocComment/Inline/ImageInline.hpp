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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_INLINE_IMAGEINLINE_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_INLINE_IMAGEINLINE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/DocComment/Inline/TextInline.hpp>
#include <string>

namespace mrdocs::doc {

/** An image.

    Syntax:

    @code
    @image html path/to/img "alt text"
    @endcode

    or

    @code
    ![Alt text](image_url "Optional title text")
    @endcode
*/
struct ImageInline final
    : InlineCommonBase<InlineKind::Image>
    , InlineContainer
{
    /** Image source URL or path.
    */
    std::string src;
    /** Alternate text when the image cannot be shown.
    */
    std::string alt;

    /** Order images by source, alt text, and inline children.
    */
    auto operator<=>(ImageInline const&) const = default;
    /** Equality compares source, alt text, and contents.
    */
    bool operator==(ImageInline const&) const noexcept = default;
};

/** Map the @ref ImageInline to a @ref dom::Object.

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
    ImageInline const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Inline const&>(I), domCorpus);
    tag_invoke(t, io, dynamic_cast<InlineContainer const&>(I), domCorpus);
    io.map("src", I.src);
    io.map("alt", I.alt);
}

/** Return the @ref ImageInline as a @ref dom::Value object.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    ImageInline const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_IMAGEINLINE_HPP
