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

#ifndef MRDOCS_API_METADATA_JAVADOC_INLINE_STYLED_HPP
#define MRDOCS_API_METADATA_JAVADOC_INLINE_STYLED_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Javadoc/Inline/Style.hpp>
#include <mrdocs/Metadata/Javadoc/Inline/Text.hpp>
#include <string>

namespace mrdocs::doc {

/** A piece of styled text.
*/
struct Styled final : Text
{
    Style style;

    static constexpr auto static_kind = NodeKind::styled;

    Styled(
        std::string string_ = std::string(),
        Style style_ = Style::none) noexcept
        : Text(std::move(string_), NodeKind::styled)
        , style(style_)
    {
    }

    auto operator<=>(Styled const&) const = default;
    bool operator==(Styled const&) const noexcept = default;
    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Styled const&>(other);
    }
};

/** Map the @ref Styled to a @ref dom::Object.

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
    Styled const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Text const&>(I), domCorpus);
    io.map("style", I.style);
}

/** Return the @ref Styled as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Styled const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_JAVADOC_INLINE_STYLED_HPP
