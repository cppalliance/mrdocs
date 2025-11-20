//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_PARAMBLOCK_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_PARAMBLOCK_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Block/BlockBase.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParagraphBlock.hpp>
#include <mrdocs/Metadata/DocComment/Block/ParamDirection.hpp>
#include <string>

namespace mrdocs::doc {

/** Documentation for a function parameter

    Syntax:

    @code
    @param name description
    @endcode
*/
struct ParamBlock final
    : BlockCommonBase<BlockKind::Param>
    , InlineContainer
{
    /** Name of the parameter being documented.
    */
    std::string name;
    /** Direction (in/out/inout) of the parameter.
    */
    ParamDirection direction = ParamDirection::none;

    /** Inherit inline container constructors.
    */
    using InlineContainer::InlineContainer;

    /** Build from an existing inline container.
    */
    ParamBlock(InlineContainer const& other)
        : InlineContainer(other)
    {}

    /** Build from an rvalue inline container.
    */
    ParamBlock(InlineContainer&& other) noexcept
        : InlineContainer(std::move(other))
    {}

    /** Construct from name, text, and direction.
        @param name Parameter identifier.
        @param text Description text.
        @param direction Flow direction of the parameter.
    */
    ParamBlock(
        std::string_view name,
        std::string_view text,
        ParamDirection direction = ParamDirection::none)
        : InlineContainer(text)
        , name(name)
        , direction(direction)
    {}

    /** Order parameter docs by name, direction, and text.
    */
    auto operator<=>(ParamBlock const&) const = default;

    /** Equality compares name, direction, and text.
    */
    bool operator==(ParamBlock const&)
        const noexcept = default;
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
    ParamBlock const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Block const&>(I), domCorpus);
    tag_invoke(t, io, dynamic_cast<InlineContainer const&>(I), domCorpus);
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
    ParamBlock const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_BLOCK_PARAMBLOCK_HPP
