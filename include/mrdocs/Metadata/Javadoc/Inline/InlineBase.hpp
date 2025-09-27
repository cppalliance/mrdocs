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

#ifndef MRDOCS_API_METADATA_JAVADOC_INLINE_INLINEBASE_HPP
#define MRDOCS_API_METADATA_JAVADOC_INLINE_INLINEBASE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Javadoc/Node/NodeBase.hpp>
#include <string>

namespace mrdocs::doc {

/** A Node containing a string of text.

    There will be no newlines in the text. Otherwise,
    this would be represented as multiple text nodes
    within a Paragraph node.
*/
struct Inline : Node
{
    constexpr ~Inline() override = default;

    bool
    isBlock() const noexcept final
    {
        return false;
    }

    auto operator<=>(Inline const&) const = default;
    bool operator==(Inline const&) const noexcept = default;
    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Inline const&>(other);
    }

protected:
    constexpr Inline() noexcept = default;

    Inline(
        NodeKind kind_)
        : Node(kind_)
    {
    }
};

/** Map the @ref Inline to a @ref dom::Object.

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
    Inline const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Node const&>(I), domCorpus);
}

/** Return the @ref Inline as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Inline const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_JAVADOC_INLINE_INLINEBASE_HPP
