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

#ifndef MRDOCS_API_METADATA_JAVADOC_BLOCK_PRECONDITION_HPP
#define MRDOCS_API_METADATA_JAVADOC_BLOCK_PRECONDITION_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Javadoc/Block/Paragraph.hpp>
#include <string>

namespace clang::mrdocs::doc {

struct Precondition final : Paragraph
{
    static constexpr NodeKind static_kind = NodeKind::precondition;

    Precondition(
        Paragraph details_ = Paragraph())
        : Paragraph(
            NodeKind::precondition,
            std::move(details_.children))
    {
    }

    using Paragraph::operator=;

    auto operator<=>(Precondition const&) const = default;

    bool operator==(Precondition const&)
        const noexcept = default;

    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Precondition const&>(other);
    }
};

/** Map the @ref Precondition to a @ref dom::Object.

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
    Precondition const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Paragraph const&>(I), domCorpus);
}

/** Return the @ref Precondition as a @ref dom::Value object.

    @param v The value to assign to.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Precondition const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // clang::mrdocs::doc

#endif // MRDOCS_API_METADATA_JAVADOC_BLOCK_PRECONDITION_HPP
