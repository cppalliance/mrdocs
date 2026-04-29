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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_INLINE_SUPERSCRIPTINLINE_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_INLINE_SUPERSCRIPTINLINE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/DocComment/Inline/TextInline.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** Superscript text fragment (raised baseline).

    Syntax:

    @code
    x^2^
    @endcode
*/
struct SuperscriptInline final
    : InlineCommonBase<InlineKind::Superscript>
    , InlineContainer
{
    /** Inherit text container constructors.
    */
    using InlineContainer::InlineContainer;
};

MRDOCS_DESCRIBE_STRUCT(
    SuperscriptInline,
    (InlineCommonBase<InlineKind::Superscript>, InlineContainer),
    ()
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_SUPERSCRIPTINLINE_HPP
