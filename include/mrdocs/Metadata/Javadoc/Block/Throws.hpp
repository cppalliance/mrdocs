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

#ifndef MRDOCS_API_METADATA_JAVADOC_BLOCK_THROWS_HPP
#define MRDOCS_API_METADATA_JAVADOC_BLOCK_THROWS_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Javadoc/Block/Paragraph.hpp>
#include <string>

namespace mrdocs::doc {

/** Documentation for a function parameter
*/
struct Throws final : Paragraph
{
    Reference exception;

    static constexpr NodeKind static_kind = NodeKind::throws;

    Throws(
        std::string exception_ = std::string(),
        Paragraph details_ = Paragraph())
        : Paragraph(
            NodeKind::throws,
            std::move(details_.children))
        , exception(std::move(exception_))
    {
    }

    using Paragraph::operator=;

    auto operator<=>(Throws const&) const = default;

    bool operator==(Throws const&)
        const noexcept = default;

    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Throws const&>(other);
    }
};

/** Map the @ref Throws to a @ref dom::Object.

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
    Throws const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Paragraph const&>(I), domCorpus);
    io.map("exception", I.exception);
}

/** Return the @ref Throws as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Throws const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_JAVADOC_BLOCK_THROWS_HPP
