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

#ifndef MRDOCS_API_METADATA_JAVADOC_INLINE_TEXT_HPP
#define MRDOCS_API_METADATA_JAVADOC_INLINE_TEXT_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Javadoc/Inline/InlineBase.hpp>
#include <string>

namespace clang::mrdocs::doc {

/** A Node containing a string of text.

    There will be no newlines in the text. Otherwise,
    this would be represented as multiple text nodes
    within a Paragraph node.
*/
struct Text : Inline
{
    std::string string;

    static constexpr auto static_kind = NodeKind::text;

    constexpr ~Text() override = default;

    constexpr Text() noexcept = default;

    explicit Text(std::string string_ = std::string()) noexcept
        : Inline(NodeKind::text)
        , string(std::move(string_))
    {}


    auto operator<=>(Text const&) const = default;
    bool operator==(Text const&) const noexcept = default;
    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Text const&>(other);
    }

protected:
    Text(
        std::string string_,
        NodeKind kind_)
        : Inline(kind_)
        , string(std::move(string_))
    {
    }
};

/** Map the @ref Text to a @ref dom::Object.

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
    Text const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Node const&>(I), domCorpus);
    io.map("string", I.string);
}

/** Return the @ref Text as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Text const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // clang::mrdocs::doc

#endif // MRDOCS_API_METADATA_JAVADOC_INLINE_TEXT_HPP
