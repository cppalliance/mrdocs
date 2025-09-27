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

#ifndef MRDOCS_API_METADATA_JAVADOC_BLOCK_POSTCONDITION_HPP
#define MRDOCS_API_METADATA_JAVADOC_BLOCK_POSTCONDITION_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Javadoc/Block/Paragraph.hpp>
#include <string>

namespace mrdocs::doc {

struct Postcondition : Paragraph
{
    static constexpr auto static_kind = NodeKind::postcondition;

    Postcondition(
        Paragraph details_ = Paragraph())
        : Paragraph(
            NodeKind::postcondition,
            std::move(details_.children))
    {
    }

    using Paragraph::operator=;

    auto operator<=>(Postcondition const&) const = default;

    bool operator==(Postcondition const&)
        const noexcept = default;

    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Postcondition const&>(other);
    }
};

/** Map the @ref Postcondition to a @ref dom::Object.

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
    Postcondition const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Paragraph const&>(I), domCorpus);
}

/** Return the @ref Postcondition as a @ref dom::Value object.

    @param v The value to assign to.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Postcondition const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_JAVADOC_BLOCK_POSTCONDITION_HPP
