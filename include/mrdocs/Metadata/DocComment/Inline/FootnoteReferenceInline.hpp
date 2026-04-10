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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_INLINE_FOOTNOTEREFERENCEINLINE_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_INLINE_FOOTNOTEREFERENCEINLINE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Inline/TextInline.hpp>
#include <mrdocs/Metadata/Symbol/SymbolID.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** A reference to a symbol.

    In markdown, this is represented as "[^label]".

    Syntax:

    @code
    [^footnote-id]
    @endcode
*/
struct FootnoteReferenceInline
    : InlineCommonBase<InlineKind::FootnoteReference>
{
    /** Footnote label that the reference points to.
    */
    std::string label;

};

MRDOCS_DESCRIBE_STRUCT(
    FootnoteReferenceInline,
    (InlineCommonBase<InlineKind::FootnoteReference>),
    (label)
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_FOOTNOTEREFERENCEINLINE_HPP
