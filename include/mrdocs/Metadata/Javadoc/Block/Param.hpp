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

#ifndef MRDOCS_API_METADATA_JAVADOC_BLOCK_PARAM_HPP
#define MRDOCS_API_METADATA_JAVADOC_BLOCK_PARAM_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Javadoc/Block/BlockBase.hpp>
#include <mrdocs/Metadata/Javadoc/Block/Paragraph.hpp>
#include <mrdocs/Metadata/Javadoc/Block/ParamDirection.hpp>
#include <string>

namespace mrdocs::doc {

/** Documentation for a function parameter
*/
struct Param final : Paragraph
{
    std::string name;
    ParamDirection direction;

    static constexpr auto static_kind = NodeKind::param;

    Param(
        std::string name_ = std::string(),
        Paragraph details_ = Paragraph(),
        ParamDirection const direction_ = ParamDirection::none)
        : Paragraph(
            NodeKind::param,
            std::move(details_.children))
        , name(std::move(name_))
        , direction(direction_)
    {
    }

    explicit
    Param(
        std::string_view const name,
        std::string_view const text)
    : Param()
    {
        this->name = name;
        this->operator=(text);
    }

    explicit
    Param(Paragraph const& other)
        : Param()
    {
        children = other.children;
    }

    Param&
    operator=(std::string_view const text) override
    {
        Paragraph::operator=(text);
        return *this;
    }

    auto operator<=>(Param const&) const = default;

    bool operator==(Param const&)
        const noexcept = default;

    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Param const&>(other);
    }
};

/** Map the @ref Param to a @ref dom::Object.

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
    Param const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Paragraph const&>(I), domCorpus);
    io.map("name", I.name);
    io.map("direction", I.direction);
}

/** Return the @ref Param as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Param const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_JAVADOC_BLOCK_PARAM_HPP
