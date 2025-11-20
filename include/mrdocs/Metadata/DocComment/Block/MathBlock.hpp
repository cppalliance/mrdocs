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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_MATHBLOCK_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_MATHBLOCK_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockBase.hpp>
#include <string>

namespace mrdocs::doc {

/** A block of LaTeX math

    A block of LaTeX math, typically between
    $$ … $$ or fenced with "math".

    On a new line:

    @code
    $$ \int_{-\infty}^{\infty} e^{-x^2} dx = \sqrt{\pi} $$
    @endcode

    Or as a code block:

    @code
    ```math
    \int_{-\infty}^{\infty} e^{-x^2} dx = \sqrt{\pi}
    ```
    @endcode
*/
struct MathBlock final
    : BlockCommonBase<BlockKind::Math>
{
    /// Raw TeX math source
    std::string literal;

    /** Copy-construct a math block.
    */
    MathBlock(MathBlock const& other) = default;

    /** Copy-assign a math block.
    */
    MathBlock& operator=(MathBlock const& other) = default;

    /** Compare math blocks by literal content.
    */
    auto operator<=>(MathBlock const&) const = default;
};

/** Map the @ref MathBlock to a @ref dom::Object.

    @param t The tag.
    @param io The output object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
*/
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    MathBlock const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Block const&>(I), domCorpus);
    io.map("literal", I.literal);
}

/** Return the @ref MathBlock as a @ref dom::Value object.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    MathBlock const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_MATHBLOCK_HPP
