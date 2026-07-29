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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_INLINE_STRIKETHROUGHINLINE_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_INLINE_STRIKETHROUGHINLINE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/DocComment/Inline/TextInline.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** Strikethrough span to show removed or deprecated text.

    Syntax:

    @code
    ~~crossed out~~
    @endcode

    When rendered to HTML, the Markdown syntax above
    typically translates into the `del` tag, which
    represents content that has been deleted or is no
    longer accurate.

    @code
    <del>This text is struck through.</del>
    @endcode

*/
struct StrikethroughInline final
    : InlineCommonBase<InlineKind::Strikethrough>
    , InlineContainer
{
    /** Inherit text container constructors.
    */
    using InlineContainer::InlineContainer;
};

MRDOCS_DESCRIBE_STRUCT(
    StrikethroughInline,
    (InlineCommonBase<InlineKind::Strikethrough>, InlineContainer),
    ()
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_STRIKETHROUGHINLINE_HPP
