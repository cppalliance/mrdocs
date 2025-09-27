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

#ifndef MRDOCS_API_METADATA_JAVADOC_BLOCK_HEADING_HPP
#define MRDOCS_API_METADATA_JAVADOC_BLOCK_HEADING_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Javadoc/Block/BlockBase.hpp>
#include <string>

namespace mrdocs::doc {

/** A manually specified section heading.
*/
struct Heading final : Block
{
    static constexpr auto static_kind = NodeKind::heading;

    std::string string;

    Heading(
        std::string string_ = std::string()) noexcept
        : Block(NodeKind::heading)
        , string(std::move(string_))
    {
    }

    auto operator<=>(Heading const&) const = default;
    bool operator==(Heading const&) const noexcept = default;
    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Heading const&>(other);
    }
};

/** Map the @ref Heading to a @ref dom::Object.

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
    Heading const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Block const&>(I), domCorpus);
    io.map("string", I.string);
}

/** Return the @ref Heading as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Heading const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_JAVADOC_BLOCK_HEADING_HPP
