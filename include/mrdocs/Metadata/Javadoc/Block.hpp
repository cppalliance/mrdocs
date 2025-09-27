//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_JAVADOC_BLOCK_HPP
#define MRDOCS_API_METADATA_JAVADOC_BLOCK_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Javadoc/Block/Admonish.hpp>
#include <mrdocs/Metadata/Javadoc/Block/Admonition.hpp>
#include <mrdocs/Metadata/Javadoc/Block/BlockBase.hpp>
#include <mrdocs/Metadata/Javadoc/Block/Brief.hpp>
#include <mrdocs/Metadata/Javadoc/Block/Code.hpp>
#include <mrdocs/Metadata/Javadoc/Block/Heading.hpp>
#include <mrdocs/Metadata/Javadoc/Block/ListItem.hpp>
#include <mrdocs/Metadata/Javadoc/Block/Paragraph.hpp>
#include <mrdocs/Metadata/Javadoc/Block/Param.hpp>
#include <mrdocs/Metadata/Javadoc/Block/ParamDirection.hpp>
#include <mrdocs/Metadata/Javadoc/Block/Postcondition.hpp>
#include <mrdocs/Metadata/Javadoc/Block/Precondition.hpp>
#include <mrdocs/Metadata/Javadoc/Block/Returns.hpp>
#include <mrdocs/Metadata/Javadoc/Block/See.hpp>
#include <mrdocs/Metadata/Javadoc/Block/TParam.hpp>
#include <mrdocs/Metadata/Javadoc/Block/Throws.hpp>
#include <mrdocs/Metadata/Javadoc/Block/UnorderedList.hpp>
#include <mrdocs/Support/Visitor.hpp>

namespace mrdocs::doc {
/** Visit a block.

    @param block The block to visit.
    @param fn The function to call for each block.
    @param args Additional arguments to pass to the function.
    @return The result of calling the function.
 */
template<
    class BlockTy,
    class Fn,
    class... Args>
    requires std::derived_from<BlockTy, Block>
decltype(auto)
visit(
    BlockTy& block,
    Fn&& fn,
    Args&&... args)
{
    auto visitor = makeVisitor<Block>(
        block, std::forward<Fn>(fn),
        std::forward<Args>(args)...);
    switch(block.Kind)
    {
    case NodeKind::admonition:
        return visitor.template visit<Admonition>();
    case NodeKind::brief:
        return visitor.template visit<Brief>();
    case NodeKind::code:
        return visitor.template visit<Code>();
    case NodeKind::heading:
        return visitor.template visit<Heading>();
    case NodeKind::paragraph:
        return visitor.template visit<Paragraph>();
    case NodeKind::list_item:
        return visitor.template visit<ListItem>();
    case NodeKind::unordered_list:
        return visitor.template visit<UnorderedList>();
    case NodeKind::param:
        return visitor.template visit<Param>();
    case NodeKind::returns:
        return visitor.template visit<Returns>();
    case NodeKind::tparam:
        return visitor.template visit<TParam>();
    case NodeKind::throws:
        return visitor.template visit<Throws>();
    case NodeKind::see:
        return visitor.template visit<See>();
    case NodeKind::precondition:
        return visitor.template visit<Precondition>();
    case NodeKind::postcondition:
        return visitor.template visit<Postcondition>();
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Traverse a list of blocks.

    @param list The list of blocks to traverse.
    @param f The function to call for each block.
    @param args Additional arguments to pass to the function.
 */
template<class F, class T, class... Args>
requires std::derived_from<T, Block>
void traverse(
    std::vector<std::unique_ptr<T>> const& list,
    F&& f, Args&&... args)
{
    for(auto const& block : list)
        visit(*block,
            std::forward<F>(f),
            std::forward<Args>(args)...);
}

/** Map the Polymorphic Block as a @ref dom::Value object.

    @param io The output parameter to receive the dom::Object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
template <class IO, polymorphic_storage_for<Block> BlockTy>
void
tag_invoke(
    dom::ValueFromTag,
    IO& io,
    BlockTy const& I,
    DomCorpus const* domCorpus)
{
    visit(*I, [&](auto const& U)
    {
        tag_invoke(
            dom::ValueFromTag{},
            io,
            U,
            domCorpus);
    });
}
} // mrdocs::doc

#endif // MRDOCS_API_METADATA_JAVADOC_BLOCK_HPP
