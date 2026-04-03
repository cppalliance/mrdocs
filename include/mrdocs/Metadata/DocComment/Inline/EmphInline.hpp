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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_INLINE_EMPHINLINE_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_INLINE_EMPHINLINE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/DocComment/Inline/InlineBase.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** Emphasized text (typically rendered in italics).

    Syntax:

    @code
    @e emphasized or *italic text* or _italic text_
    @endcode
*/
struct EmphInline final
    : InlineCommonBase<InlineKind::Emph>
    , InlineContainer
{
    /** Inherit inline container constructors.
    */
    using InlineContainer::InlineContainer;

    /** Order emphasis spans by their contents.
    */
    auto operator<=>(EmphInline const&) const = default;

    /** Equality compares contained text.
    */
    bool operator==(EmphInline const&) const noexcept = default;
};

MRDOCS_DESCRIBE_STRUCT(
    EmphInline,
    (InlineCommonBase<InlineKind::Emph>, InlineContainer),
    ()
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_EMPHINLINE_HPP
