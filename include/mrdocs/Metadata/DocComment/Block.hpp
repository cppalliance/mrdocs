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
#include <mrdocs/Support/Visitor.hpp>

namespace mrdocs::doc {
/** Visit a block.

    @param block The block to visit.
    @param fn The function to call for each block.
    @param args Additional arguments to pass to the function.
    @return The result of calling the function.
 */
template<
    std::derived_from<Block> BlockTy,
    class Fn,
    class... Args>
decltype(auto)
visit(
    BlockTy& info,
    Fn&& fn,
    Args&&... args)
{
    auto visitor = makeVisitor<Block>(
        info, std::forward<Fn>(fn), std::forward<Args>(args)...);
    switch(info.Kind)
    {
    #define INFO(Type) case BlockKind::Type: \
        return visitor.template visit<Type##Block>();
#include <mrdocs/Metadata/DocComment/Block/BlockNodes.inc>
    default:
        MRDOCS_UNREACHABLE();
    }
}

MRDOCS_DECL
std::strong_ordering
operator<=>(Polymorphic<Block> const& lhs, Polymorphic<Block> const& rhs);

inline bool
operator==(Polymorphic<Block> const& lhs, Polymorphic<Block> const& rhs)
{
    return lhs <=> rhs == std::strong_ordering::equal;
}

/** Map the Polymorphic Block to a @ref dom::Object.

    @param io The output parameter to receive the dom::Object.
    @param I The polymorphic Block to convert.
    @param domCorpus The DomCorpus used to resolve references.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    Polymorphic<Block> const& I,
    DomCorpus const* domCorpus)
{
    visit(*I, [&](auto const& U)
    {
        tag_invoke(
            dom::LazyObjectMapTag{},
            io,
            U,
            domCorpus);
    });
}

/** Map the Polymorphic Block as a @ref dom::Value object.

    @param io The output parameter to receive the dom::Value.
    @param I The polymorphic Block to convert.
    @param domCorpus The DomCorpus used to resolve references.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Polymorphic<Block> const& I,
    DomCorpus const* domCorpus)
{
    visit(*I, [&](auto const& U)
    {
        tag_invoke(
            dom::ValueFromTag{},
            v,
            U,
            domCorpus);
    });
}


inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Optional<Polymorphic<Block>> const& I,
    DomCorpus const* domCorpus)
{
    if (!I)
    {
        v = nullptr;
        return;
    }
    MRDOCS_ASSERT(!I->valueless_after_move());
    tag_invoke(dom::ValueFromTag{}, v, *I, domCorpus);
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
