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

#include <mrdocs/Metadata/DocComment/Block.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockBase.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Support/Algorithm.hpp>
#include <mrdocs/Support/String.hpp>
#include <iostream>

namespace mrdocs {
namespace doc {

void
ltrim(Block& el)
{
    visit(el, []<class BlockTy>(BlockTy& N)
    {
        if constexpr (std::derived_from<BlockTy, BlockContainer>)
        {
            ltrim(N.asBlockContainer());
        }
        if constexpr (std::derived_from<BlockTy, InlineContainer>)
        {
            ltrim(N.asInlineContainer());
        }
        if constexpr (requires { { N.literal } -> std::same_as<std::string>; })
        {
            std::string& text = N.literal;
            text = mrdocs::ltrim(text);
        }
    });
}

void
rtrim(Block& el)
{
    visit(el, []<class BlockTy>(BlockTy& N) {
        if constexpr (std::derived_from<BlockTy, BlockContainer>)
        {
            rtrim(N.asBlockContainer());
        }
        else if constexpr (std::derived_from<BlockTy, InlineContainer>)
        {
            rtrim(N.asInlineContainer());
        }
        else if constexpr (requires { { N.literal } -> std::same_as<std::string>; })
        {
            std::string& text = N.literal;
            text = mrdocs::rtrim(text);
        }
    });
}

bool
isEmpty(Block const& el)
{
    return visit(el, []<class BlockTy>(BlockTy const& N) -> bool
    {
        if constexpr (std::derived_from<BlockTy, BlockContainer>)
        {
            return N.blocks.empty();
        }
        else if constexpr (std::derived_from<BlockTy, InlineContainer>)
        {
            return N.children.empty();
        }
        else if constexpr (requires { { N.literal } -> std::same_as<std::string>; })
        {
            return N.literal.empty();
        }
        else
        {
            return is_one_of(
                N.Kind,
                {BlockKind::ThematicBreak});
        }
    });
}


std::strong_ordering
BlockContainer::
operator<=>(BlockContainer const& rhs) const
{
    return this->blocks <=> rhs.blocks;
}

void
ltrim(BlockContainer& container)
{
    for (auto it = container.blocks.begin(); it != container.blocks.end();)
    {
        auto& child = *it;
        visit(*child, []<class BlockTy>(BlockTy& N) {
            if constexpr (std::derived_from<BlockTy, BlockContainer>)
            {
                ltrim(N.asBlockContainer());
            }
            else if constexpr (std::same_as<TableBlock, BlockTy>)
            {
                for (TableRow& row : N.items)
                {
                    for (TableCell& cell : row.Cells)
                    {
                        ltrim(cell.asInlineContainer());
                        if (!cell.children.empty())
                        {
                            break;
                        }
                    }
                }
            }
            else if constexpr (std::same_as<ListBlock, BlockTy>)
            {
                for (ListItem& item : N.items)
                {
                    ltrim(item.asBlockContainer());
                    if (!item.blocks.empty())
                    {
                        break;
                    }
                }
            }
            else if constexpr (requires { { N.literal } -> std::same_as<std::string>; })
            {
                std::string& text = N.literal;
                text = mrdocs::ltrim(text);
            }
        });
        if (!isEmpty(child))
        {
            break;
        }
        it = container.blocks.erase(it);
    }
}

void
rtrim(BlockContainer& container)
{
    for (auto it = container.blocks.rbegin(); it != container.blocks.rend();)
    {
        auto& child = *it;
        visit(*child, []<class BlockTy>(BlockTy& N) {
            if constexpr (std::derived_from<BlockTy, BlockContainer>)
            {
                rtrim(N.asBlockContainer());
            }
            else if constexpr (std::same_as<TableBlock, BlockTy>)
            {
                for (TableRow& row : N.items)
                {
                    for (TableCell& cell : row.Cells)
                    {
                        ltrim(cell.asInlineContainer());
                        if (!cell.children.empty())
                        {
                            break;
                        }
                    }
                }
            }
            else if constexpr (std::same_as<ListBlock, BlockTy>)
            {
                for (ListItem& item: N.items)
                {
                    rtrim(item.asBlockContainer());
                    if (!item.blocks.empty())
                    {
                        break;
                    }
                }
            }
            else if constexpr (requires { { N.literal } -> std::same_as<std::string>; })
            {
                std::string& text = N.literal;
                text = mrdocs::rtrim(text);
            }
        });
        if (!isEmpty(child))
        {
            break;
        }
        it = decltype(it){ container.blocks.erase(std::next(it).base()) };
    }
}


}
} // mrdocs