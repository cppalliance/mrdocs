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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_INLINE_MATHINLINE_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_INLINE_MATHINLINE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Inline/InlineBase.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>

namespace mrdocs::doc {

/** An inline LaTeX math expression

    Inline LaTeX math, typically between $…$.

    Syntax:

    @code
    $2 + 2 = 4$ or $x_{i+1}^2$
    @endcode
*/
struct MathInline
    : InlineCommonBase<InlineKind::Math>
{
    /** Raw LaTeX/TeX math content.
    */
    std::string literal;

    /** Virtual destructor for inline hierarchy.
    */
    constexpr ~MathInline() override = default;
    /** Construct an empty math inline.
    */
    constexpr MathInline() noexcept = default;

    /** Construct a math inline from source text.
    */
    explicit MathInline(std::string string_) noexcept
        : literal(std::move(string_))
    {}

    /** Order math spans by their literal content.
    */
    auto operator<=>(MathInline const&) const = default;
    /** Equality compares literal content.
    */
    bool operator==(MathInline const&) const noexcept = default;
};

MRDOCS_DESCRIBE_STRUCT(
    MathInline,
    (InlineCommonBase<InlineKind::Math>),
    (literal)
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_MATHINLINE_HPP
