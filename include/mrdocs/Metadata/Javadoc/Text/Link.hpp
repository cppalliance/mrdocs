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

#ifndef MRDOCS_API_METADATA_JAVADOC_TEXT_LINK_HPP
#define MRDOCS_API_METADATA_JAVADOC_TEXT_LINK_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Javadoc/Text/TextBase.hpp>
#include <string>

namespace clang::mrdocs::doc {

/** A hyperlink.
*/
struct Link final : Text
{
    std::string href;

    static constexpr auto static_kind = NodeKind::link;

    explicit
    Link(
        std::string string_ = std::string(),
        std::string href_ = std::string()) noexcept
        : Text(std::move(string_), NodeKind::link)
        , href(std::move(href_))
    {
    }

    auto operator<=>(Link const&) const = default;
    bool operator==(Link const&) const noexcept = default;
    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Link const&>(other);
    }
};

/** Map the @ref Link to a @ref dom::Object.

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
    Link const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Text const&>(I), domCorpus);
    io.map("href", I.href);
}

/** Return the @ref Link as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Link const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // clang::mrdocs::doc

#endif // MRDOCS_API_METADATA_JAVADOC_TEXT_LINK_HPP
