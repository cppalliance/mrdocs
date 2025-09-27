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

#ifndef MRDOCS_API_METADATA_JAVADOC_BLOCK_CODE_HPP
#define MRDOCS_API_METADATA_JAVADOC_BLOCK_CODE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Javadoc/Block/BlockBase.hpp>
#include <mrdocs/Metadata/Javadoc/Block/Paragraph.hpp>
#include <string>

namespace mrdocs::doc {

/** Preformatted source code.
 */
struct Code final : Paragraph
{
    // VFALCO we can add a language (e.g., C++),
    //        then emit attributes in the generator.

    static constexpr auto static_kind = NodeKind::code;

    Code() noexcept
        : Paragraph(NodeKind::code)
    {
    }

    using Paragraph::operator=;
    auto operator<=>(Code const&) const = default;
    bool operator==(Code const&) const noexcept = default;
    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
               *this == dynamic_cast<Code const&>(other);
    }
};

/** Map the @ref Code to a @ref dom::Object.

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
    Code const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Paragraph const&>(I), domCorpus);
}

/** Return the @ref Code as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Code const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_JAVADOC_BLOCK_CODE_HPP
