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

#ifndef MRDOCS_API_METADATA_JAVADOC_BLOCK_LISTITEM_HPP
#define MRDOCS_API_METADATA_JAVADOC_BLOCK_LISTITEM_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Javadoc/Block/BlockBase.hpp>
#include <mrdocs/Metadata/Javadoc/Block/Paragraph.hpp>
#include <string>

namespace clang::mrdocs::doc {

/** An item in a list
*/
struct ListItem final : Paragraph
{
    static constexpr auto static_kind = NodeKind::list_item;

    ListItem()
        : Paragraph(static_kind)
    {}

    using Paragraph::operator=;

    auto operator<=>(ListItem const&) const = default;
    bool
    operator==(ListItem const&) const noexcept = default;

    bool
    equals(Node const& other) const noexcept override
    {
        auto* p = dynamic_cast<ListItem const*>(&other);
        if (!p)
        {
            return false;
        }
        if (this == &other)
        {
            return true;
        }
        return *this == *p;
    }
};

/** Map the @ref ListItem to a @ref dom::Object.

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
    ListItem const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Paragraph const&>(I), domCorpus);
}

/** Return the @ref ListItem as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    ListItem const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // clang::mrdocs::doc

#endif // MRDOCS_API_METADATA_JAVADOC_BLOCK_LISTITEM_HPP
