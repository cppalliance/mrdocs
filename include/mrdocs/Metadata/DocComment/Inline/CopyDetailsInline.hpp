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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_INLINE_COPYDETAILSINLINE_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_INLINE_COPYDETAILSINLINE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Inline/ReferenceInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/TextInline.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** Documentation copied from another symbol.

    Syntax:

    @code
    @copydetails other_symbol
    @endcode
*/
struct CopyDetailsInline final
    : InlineCommonBase<InlineKind::CopyDetails>
{
    /** Element to copy the details from
    */
    std::string string;
    /** Symbol to copy details from.
    */
    SymbolID id = SymbolID::invalid;

    /** Construct with optional text payload.
    */
    CopyDetailsInline(std::string string_ = std::string()) noexcept
        : string(std::move(string_))
    {
    }

    /** Order copy directives by text and symbol id.
    */
    auto operator<=>(CopyDetailsInline const&) const = default;
    /** Equality compares text and symbol id.
    */
    bool operator==(CopyDetailsInline const&) const noexcept = default;
};

MRDOCS_DESCRIBE_STRUCT(
    CopyDetailsInline,
    (Inline),
    (string, id)
)

/** Map the @ref CopyDetailsInline to a @ref dom::Object.

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
    CopyDetailsInline const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Inline const&>(I), domCorpus);
    io.map("string", I.string);
    io.map("symbol", I.id);
}

/** Return the @ref CopyDetailsInline as a @ref dom::Value object.

    @param v The output value.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    CopyDetailsInline const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_COPYDETAILSINLINE_HPP
