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

#ifndef MRDOCS_API_METADATA_JAVADOC_BLOCK_UNORDEREDLIST_HPP
#define MRDOCS_API_METADATA_JAVADOC_BLOCK_UNORDEREDLIST_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Javadoc/Block/BlockBase.hpp>
#include <mrdocs/Metadata/Javadoc/Block/ListItem.hpp>
#include <mrdocs/Metadata/Javadoc/Block/Paragraph.hpp>
#include <string>
#include <vector>

namespace mrdocs::doc {

/** A list of list items
*/
struct UnorderedList final : Paragraph
{
    static constexpr auto static_kind = NodeKind::unordered_list;

    std::vector<ListItem> items;

    UnorderedList()
        : Paragraph(static_kind)
    {
    }

    using Paragraph::operator=;

    auto operator<=>(UnorderedList const& other) const {
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
    operator==(UnorderedList const&) const noexcept = default;

    bool
    equals(Node const& other) const noexcept override
    {
        auto* p = dynamic_cast<UnorderedList const*>(&other);
        if (!p)
        {
            return false;
        }
        if (this == &other)
        {
            return true;
        }
        return *this == *p;
    }
};

/** Map the @ref UnorderedList to a @ref dom::Object.

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
    UnorderedList const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Paragraph const&>(I), domCorpus);
    io.defer("items", [&I, domCorpus] {
        return dom::LazyArray(I.items, domCorpus);
    });
}

/** Return the @ref UnorderedList as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    UnorderedList const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_JAVADOC_BLOCK_UNORDEREDLIST_HPP
