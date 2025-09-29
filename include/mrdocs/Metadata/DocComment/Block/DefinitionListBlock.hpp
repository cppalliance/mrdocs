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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_DEFINITIONLISTBLOCK_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_DEFINITIONLISTBLOCK_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockBase.hpp>
#include <mrdocs/Metadata/DocComment/Block/DefinitionListItem.hpp>
#include <mrdocs/Metadata/DocComment/Block/ListKind.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParagraphBlock.hpp>
#include <string>
#include <vector>

namespace mrdocs::doc {

struct DefinitionListBlock final
    : BlockCommonBase<BlockKind::List>
{
    std::vector<DefinitionListItem> items;

    auto operator<=>(DefinitionListBlock const& other) const {
        if (auto const cmp = items.size() <=> other.items.size();
            !std::is_eq(cmp))
        {
            return cmp;
        }
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            if (auto const cmp = items[i] <=> other.items[i];
                !std::is_eq(cmp))
            {
                return cmp;
            }
        }
        return std::strong_ordering::equal;
    }

    bool
    operator==(DefinitionListBlock const&) const noexcept = default;
};

template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    DefinitionListBlock const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Block const&>(I), domCorpus);
    io.defer("items", [&I, domCorpus] {
        return dom::LazyArray(I.items, domCorpus);
    });
}

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    DefinitionListBlock const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_DEFINITIONLISTBLOCK_HPP
