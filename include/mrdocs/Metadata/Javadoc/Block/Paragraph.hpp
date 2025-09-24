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

#ifndef MRDOCS_API_METADATA_JAVADOC_BLOCK_PARAGRAPH_HPP
#define MRDOCS_API_METADATA_JAVADOC_BLOCK_PARAGRAPH_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Javadoc/Block/BlockBase.hpp>
#include <string>

namespace clang::mrdocs::doc {

/** A sequence of text nodes.
*/
struct Paragraph : Block
{
    static constexpr auto static_kind = NodeKind::paragraph;

    Paragraph() noexcept : Block(NodeKind::paragraph) {}

    virtual
    Paragraph&
    operator=(std::string_view str);

    auto operator<=>(Paragraph const&) const = default;

    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Paragraph const&>(other);
    }

protected:
    explicit
    Paragraph(
        NodeKind const kind,
        std::vector<Polymorphic<Text>> children_ = {}) noexcept
        : Block(kind, std::move(children_))
    {
    }
};

/** Map the @ref Paragraph to a @ref dom::Object.

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
    Paragraph const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Block const&>(I), domCorpus);
}

/** Return the @ref Paragraph as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Paragraph const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // clang::mrdocs::doc

#endif // MRDOCS_API_METADATA_JAVADOC_BLOCK_PARAGRAPH_HPP
