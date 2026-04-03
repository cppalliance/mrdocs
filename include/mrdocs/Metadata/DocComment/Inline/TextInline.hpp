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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_INLINE_TEXTINLINE_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_INLINE_TEXTINLINE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Inline/InlineBase.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** A Node containing a string of text.

    There will be no newlines in the text. Otherwise,
    this would be represented as multiple text nodes
    within a Paragraph node.

    Syntax:

    @code
    plain text
    @endcode
*/
struct TextInline
    : InlineCommonBase<InlineKind::Text>
{
    /** Plain text carried by this inline node.
    */
    std::string literal;

    /** Virtual destructor for the inline hierarchy.
    */
    constexpr ~TextInline() override = default;

    /** Construct an empty text inline.
    */
    constexpr TextInline() noexcept = default;

    /** Construct from a string view.
    */
    explicit TextInline(std::string_view str) noexcept
        : literal(str)
    {}

    /** Construct from a C string.
    */
    explicit TextInline(char const* str) noexcept
        : literal(str)
    {}

    /** Construct from a string copy.
    */
    explicit TextInline(std::string const& str) noexcept
        : literal(str)
    {}

    /** Construct by moving text storage.
    */
    explicit TextInline(std::string&& str) noexcept
        : literal(std::move(str))
    {}

    /** Order text nodes lexicographically by their literal.
    */
    auto operator<=>(TextInline const&) const = default;
    /** Equality compares literal strings.
    */
    bool operator==(TextInline const&) const noexcept = default;
};

MRDOCS_DESCRIBE_STRUCT(
    TextInline,
    (InlineCommonBase<InlineKind::Text>),
    (literal)
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_TEXTINLINE_HPP
