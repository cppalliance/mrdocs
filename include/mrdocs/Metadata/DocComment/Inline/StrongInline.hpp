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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_INLINE_STRONGINLINE_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_INLINE_STRONGINLINE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/DocComment/Inline/TextInline.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** Strong emphasis span (typically rendered in bold).

    Syntax:

    @code
    @b bold
    @endcode
*/
struct StrongInline final
    : InlineCommonBase<InlineKind::Strong>
    , InlineContainer
{
    /** Inherit text container constructors.
    */
    using InlineContainer::InlineContainer;
    /** Order strong spans by their children.
    */
    auto operator<=>(StrongInline const&) const = default;
    /** Equality compares contained inline children.
    */
    bool operator==(StrongInline const&) const noexcept = default;
};

MRDOCS_DESCRIBE_STRUCT(
    StrongInline,
    (InlineCommonBase<InlineKind::Strong>, InlineContainer),
    ()
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_STRONGINLINE_HPP
