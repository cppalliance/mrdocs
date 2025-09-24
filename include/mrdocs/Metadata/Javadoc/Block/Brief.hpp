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

#ifndef MRDOCS_API_METADATA_JAVADOC_BLOCK_BRIEF_HPP
#define MRDOCS_API_METADATA_JAVADOC_BLOCK_BRIEF_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Javadoc/Block/BlockBase.hpp>
#include <mrdocs/Metadata/Javadoc/Block/Paragraph.hpp>
#include <mrdocs/Metadata/Javadoc/Node/NodeBase.hpp>
#include <string>

namespace clang::mrdocs::doc {

/** The brief description
*/
struct Brief final : Paragraph
{
    static constexpr NodeKind static_kind = NodeKind::brief;

    std::vector<std::string> copiedFrom;

    Brief() noexcept
        : Paragraph(NodeKind::brief)
    {
    }

    explicit
    Brief(std::string_view const text)
        : Brief()
    {
        operator=(text);
    }

    Brief(Brief const& other) = default;

    Brief&
    operator=(Brief const& other) = default;

    Brief&
    operator=(std::string_view const text) override
    {
        Paragraph::operator=(text);
        return *this;
    }

    auto operator<=>(Brief const&) const = default;

    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Brief const&>(other);
    }
};

/** Map the @ref Brief to a @ref dom::Object.

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
    Brief const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Paragraph const&>(I), domCorpus);
}

/** Return the @ref Brief as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Brief const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // clang::mrdocs::doc

#endif // MRDOCS_API_METADATA_JAVADOC_BLOCK_BRIEF_HPP
