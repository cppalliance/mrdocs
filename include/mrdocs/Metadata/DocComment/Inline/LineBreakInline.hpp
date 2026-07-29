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
#include <mrdocs/Support/Reflection/Describe.hpp>
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

};

MRDOCS_DESCRIBE_STRUCT(
    LineBreakInline,
    (InlineCommonBase<InlineKind::LineBreak>),
    ()
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_LINEBREAKINLINE_HPP
