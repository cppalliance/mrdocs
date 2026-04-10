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

};

MRDOCS_DESCRIBE_STRUCT(
    LinkInline,
    (InlineCommonBase<InlineKind::Link>, InlineContainer),
    (href)
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_LINKINLINE_HPP
