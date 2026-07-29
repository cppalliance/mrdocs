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
#include <mrdocs/Support/Reflection/Describe.hpp>
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
    /** Inherit text container constructors.
    */
    using InlineContainer::InlineContainer;
};

MRDOCS_DESCRIBE_STRUCT(
    SubscriptInline,
    (InlineCommonBase<InlineKind::Subscript>, InlineContainer),
    ()
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_SUBSCRIPTINLINE_HPP
