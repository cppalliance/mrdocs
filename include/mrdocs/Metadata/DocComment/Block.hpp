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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Block/AdmonitionBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/BriefBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/CodeBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/DefinitionListBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/FootnoteDefinitionBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/HeadingBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/ListBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/MathBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParagraphBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParamBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/PostconditionBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/PreconditionBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/QuoteBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/ReturnsBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/SeeBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/TParamBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/TableBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/ThematicBreakBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/ThrowsBlock.hpp>
#include <mrdocs/Support/TypeTraits/Visitor.hpp>

/** Doc-comment enums and helpers that describe admonition kinds. */
namespace mrdocs::doc {

// Register Block's concrete kinds for the generic visit
// (Support/Reflection/Describe.hpp).
#define INFO(X) MRDOCS_KIND_ENTRY(Block, X##Block)
MRDOCS_DESCRIBE_KINDS_BEGIN(Block)
#include <mrdocs/Metadata/DocComment/Block/BlockNodes.inc>
MRDOCS_DESCRIBE_KINDS_END(Block)
#undef INFO

/** Three-way comparison between polymorphic block wrappers.
*/
MRDOCS_DECL
std::strong_ordering
operator<=>(Polymorphic<Block> const& lhs, Polymorphic<Block> const& rhs);

/** Equality delegates to the three-way comparison.
*/
inline bool
operator==(Polymorphic<Block> const& lhs, Polymorphic<Block> const& rhs)
{
    return lhs <=> rhs == std::strong_ordering::equal;
}


/** Removes leading whitespace from the block.

    @param el The Polymorphic<Block> to trim.
    @return void
*/
inline
void
ltrim(Polymorphic<Block>& el)
{
    ltrim(*el);
}

/** Removes trailing whitespace from the block.

    @param el The Polymorphic<Block> to trim.
    @return void
*/
inline
void
rtrim(Polymorphic<Block>& el)
{
    rtrim(*el);
}

/** Removes leading and trailing whitespace from the block.

    @param el The Polymorphic<Block> to trim.
    @return void
*/
inline
void
trim(Polymorphic<Block>& el)
{
    ltrim(el);
    rtrim(el);
}

/** Determine if the inline is empty
*/
inline
bool
isEmpty(Polymorphic<Block> const& el)
{
    MRDOCS_ASSERT(!el.valueless_after_move());
    return isEmpty(*el);
}


} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_HPP
