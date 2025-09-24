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

#ifndef MRDOCS_API_METADATA_JAVADOC_BLOCK_ADMONITION_HPP
#define MRDOCS_API_METADATA_JAVADOC_BLOCK_ADMONITION_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Javadoc/Block/Admonish.hpp>
#include <mrdocs/Metadata/Javadoc/Block/BlockBase.hpp>
#include <mrdocs/Metadata/Javadoc/Block/Paragraph.hpp>
#include <string>

namespace clang::mrdocs::doc {

/** An admonition.

    This paragraph represents an admonition, such as
    a note, tip, important, caution, or warning.

*/
struct Admonition final : Paragraph
{
    Admonish admonish;

    explicit
    Admonition(
        Admonish const admonish_ = Admonish::none) noexcept
        : Paragraph(NodeKind::admonition)
        , admonish(admonish_)
    {
    }

    using Paragraph::operator=;

    auto operator<=>(Admonition const&) const = default;

    bool operator==(Admonition const&) const noexcept = default;

    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Admonition const&>(other);
    }
};

/** Map the @ref Admonition to a @ref dom::Object.

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
    Admonition const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Paragraph const&>(I), domCorpus);
    io.map("admonish", I.admonish);
}

/** Return the @ref Admonition as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Admonition const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // clang::mrdocs::doc

#endif // MRDOCS_API_METADATA_JAVADOC_BLOCK_ADMONITION_HPP
