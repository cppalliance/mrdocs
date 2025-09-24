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

#ifndef MRDOCS_API_METADATA_JAVADOC_NODE_NODEBASE_HPP
#define MRDOCS_API_METADATA_JAVADOC_NODE_NODEBASE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom/LazyObject.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Metadata/Javadoc/Node/NodeKind.hpp>

namespace clang::mrdocs::doc {

//--------------------------------------------

/** This is a variant-like list element.

    There are two types of nodes: text and block.

    - The javadoc is a list of blocks.
    - A block contains a list of text elements.
    - A text element contains a string.
*/
struct MRDOCS_DECL
    Node
{
    NodeKind Kind;

    virtual ~Node() = default;

    explicit Node(NodeKind const kind_) noexcept
        : Kind(kind_)
    {
    }

    virtual bool isBlock() const noexcept = 0;

    bool isText() const noexcept
    {
        return ! isBlock();
    }

    auto operator<=>(Node const&) const = default;
    bool operator==(Node const&)const noexcept = default;
    virtual bool equals(Node const& other) const noexcept
    {
        return Kind == other.Kind;
    }
};

/** Map the @ref Node to a @ref dom::Object.

    @param io The output parameter to receive the dom::Object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    Node const& I,
    DomCorpus const* domCorpus)
{
    MRDOCS_ASSERT(domCorpus);
    io.map("kind", I.Kind);
}

/** Return the @ref Node as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Node const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // clang::mrdocs::doc

#endif // MRDOCS_API_METADATA_JAVADOC_NODE_NODEBASE_HPP
