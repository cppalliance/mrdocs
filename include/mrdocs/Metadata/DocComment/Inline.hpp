//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_INLINE_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_INLINE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/DocComment/Inline/CodeInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/CopyDetailsInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/EmphInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/FootnoteReferenceInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/HighlightInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/ImageInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/LineBreakInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/LinkInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/MathInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/ReferenceInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/SoftBreakInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/StrikethroughInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/StrongInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/SubscriptInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/SuperscriptInline.hpp>
#include <mrdocs/Metadata/DocComment/Inline/TextInline.hpp>
#include <mrdocs/Support/TypeTraits/Concepts.hpp>
#include <mrdocs/Support/TypeTraits/Visitor.hpp>
#include <compare>

namespace mrdocs::doc {

// Register Inline's concrete kinds for the generic visit
// (Support/Reflection/Describe.hpp).
#define INFO(X) MRDOCS_KIND_ENTRY(Inline, X##Inline)
MRDOCS_DESCRIBE_KINDS_BEGIN(Inline)
#include <mrdocs/Metadata/DocComment/Inline/InlineNodes.inc>
MRDOCS_DESCRIBE_KINDS_END(Inline)
#undef INFO

/** Three-way comparison for polymorphic inline elements.
*/
MRDOCS_DECL
std::strong_ordering
operator<=>(Polymorphic<Inline> const& lhs, Polymorphic<Inline> const& rhs);

/** Equality delegates to the three-way comparison.
*/
inline
bool
operator==(Polymorphic<Inline> const& lhs, Polymorphic<Inline> const& rhs) {
    return std::is_eq(lhs <=> rhs);
}

/** Removes leading whitespace from the inline element.

    @param el The Polymorphic<Inline> to trim.
    @return void
*/
MRDOCS_DECL
void
ltrim(Polymorphic<Inline>& el);

/** Removes trailing whitespace from the inline element.

    @param el The Polymorphic<Inline> to trim.
    @return void
*/
MRDOCS_DECL
void
rtrim(Polymorphic<Inline>& el);

/** Removes leading and trailing whitespace from the inline element.

    @param el The Polymorphic<Inline> to trim.
    @return void
*/
inline
void
trim(Polymorphic<Inline>& el)
{
    ltrim(el);
    rtrim(el);
}

/** Determine if the inline is empty

    This determines if the inline is considered to
    have no content for the purposes of trimming.

*/
MRDOCS_DECL
bool
isEmpty(Polymorphic<Inline> const& el);

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_HPP
