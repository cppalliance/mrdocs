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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_INLINE_HIGHLIGHTINLINE_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_INLINE_HIGHLIGHTINLINE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/DocComment/Inline/InlineBase.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** Highlighted text span used to call out important words.

    Syntax:

    @code
    ==highlight== or <mark>highlighted</mark>
    @endcode
*/
struct HighlightInline final
    : InlineCommonBase<InlineKind::Highlight>
    , InlineContainer
{
    /** Order highlights by their inline content.
    */
    auto operator<=>(HighlightInline const&) const = default;
    /** Equality compares inline content.
    */
    bool operator==(HighlightInline const&) const noexcept = default;
};

MRDOCS_DESCRIBE_STRUCT(
    HighlightInline,
    (InlineCommonBase<InlineKind::Highlight>, InlineContainer),
    ()
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_HIGHLIGHTINLINE_HPP
