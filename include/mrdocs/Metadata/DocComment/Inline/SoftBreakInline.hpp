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
#include <mrdocs/Support/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** A line break that may render as a space

    Syntax:

    @code
    This is the first part of a line,
    and this is the continuation on the next line.
    @endcode

    Placing a backslash (`\`) at the end of a line,
    followed by a new line, can also create a soft line break.
    This method is often preferred because it is less susceptible
    to space-trimming issues.

    @code
    This is the first part of a line,\
    and this is the continuation on the next line.
    @endcode
*/
struct SoftBreakInline
    : InlineCommonBase<InlineKind::SoftBreak>
{
    /** Virtual destructor for inline hierarchy.
    */
    constexpr ~SoftBreakInline() override = default;
    /** Construct a soft line break node.
    */
    constexpr SoftBreakInline() noexcept = default;
};

MRDOCS_DESCRIBE_STRUCT(
    SoftBreakInline,
    (InlineCommonBase<InlineKind::SoftBreak>),
    ()
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_SOFTBREAKINLINE_HPP
