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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_INLINE_CODEINLINE_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_INLINE_CODEINLINE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/DocComment/Inline/InlineBase.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** Inline code (monospaced) used for short snippets within text.

    Syntax:

    @code
    @c inline_code
    @endcode

    or

    @code
    `inline_code`
    @endcode
*/
struct CodeInline final
    : InlineCommonBase<InlineKind::Code>
    , InlineContainer
{
    /** Inherit inline container constructors.
    */
    using InlineContainer::InlineContainer;
};

MRDOCS_DESCRIBE_STRUCT(
    CodeInline,
    (InlineCommonBase<InlineKind::Code>, InlineContainer),
    ()
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_CODEINLINE_HPP
