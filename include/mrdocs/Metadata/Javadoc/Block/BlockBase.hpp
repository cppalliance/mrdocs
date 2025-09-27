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

#ifndef MRDOCS_API_METADATA_JAVADOC_BLOCK_BLOCKBASE_HPP
#define MRDOCS_API_METADATA_JAVADOC_BLOCK_BLOCKBASE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/Dom/LazyObject.hpp>
#include <mrdocs/Metadata/Javadoc/Inline.hpp>
#include <mrdocs/Metadata/Javadoc/Node/NodeBase.hpp>
#include <algorithm>
#include <string>

namespace mrdocs::doc {

/** A piece of block content

    The top level is a list of blocks.

    There are two types of blocks: headings and paragraphs
*/
struct MRDOCS_DECL
    Block : Node
{
    std::vector<Polymorphic<Text>> children;

    bool isBlock() const noexcept final
    {
        return true;
    }

    bool empty() const noexcept
    {
        return children.empty();
    }

    auto operator<=>(Block const& other) const {
        if (auto const cmp = children.size() <=> other.children.size();
            !std::is_eq(cmp))
        {
            return cmp;
        }
        for (std::size_t i = 0; i < children.size(); ++i)
        {
            if (auto const cmp = *children[i] <=> *other.children[i];
                !std::is_eq(cmp))
            {
                return cmp;
            }
        }
        return std::strong_ordering::equal;
    }

    bool operator==(Block const& other) const noexcept
    {
        if (Kind != other.Kind)
        {
            return false;
        }
        return std::ranges::
            equal(children, other.children,
            [](auto const& a, auto const& b)
            {
                return a->equals(*b);
            });
    }

    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Block const&>(other);
    }

    template<std::derived_from<Text> T>
    T& emplace_back(T&& text)
    {
        return static_cast<T&>(emplace_back(
            std::make_unique<T>(std::forward<T>(text))));
    }

    void append(std::vector<Polymorphic<Node>>&& blocks);

    void append(std::vector<Polymorphic<Text>> const& otherChildren);

protected:
    constexpr
    Block() = default;

    explicit
    Block(
        NodeKind const kind_,
        std::vector<Polymorphic<Text>> children_ = {}) noexcept
        : Node(kind_)
        , children(std::move(children_))
    {
    }

private:
    Text& emplace_back(Polymorphic<Text> text);
};

/** Map the @ref Block to a @ref dom::Object.

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
    Block const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Node const&>(I), domCorpus);
    io.defer("children", [&I, domCorpus] {
        return dom::LazyArray(I.children, domCorpus);
    });
}

/** Return the @ref Block as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Block const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_JAVADOC_BLOCK_BLOCKBASE_HPP
