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

#ifndef MRDOCS_API_METADATA_JAVADOC_TEXT_REFERENCE_HPP
#define MRDOCS_API_METADATA_JAVADOC_TEXT_REFERENCE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Info/SymbolID.hpp>
#include <mrdocs/Metadata/Javadoc/Text/TextBase.hpp>
#include <string>

namespace clang::mrdocs::doc {

/** A reference to a symbol.
*/
struct Reference : Text
{
    SymbolID id = SymbolID::invalid;

    static constexpr auto static_kind = NodeKind::reference;

    explicit
    Reference(
        std::string string_ = std::string()) noexcept
        : Text(std::move(string_), NodeKind::reference)
    {
    }

    auto operator<=>(Reference const&) const = default;
    bool operator==(Reference const&) const noexcept = default;
    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Reference const&>(other);
    }

protected:
    Reference(
        std::string string_,
        NodeKind const kind_) noexcept
        : Text(std::move(string_), kind_)
    {
    }
};

/** Map the @ref Reference to a @ref dom::Object.

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
    Reference const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Text const&>(I), domCorpus);
    io.map("symbol", I.id);
}

/** Return the @ref Reference as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Reference const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // clang::mrdocs::doc

#endif // MRDOCS_API_METADATA_JAVADOC_TEXT_REFERENCE_HPP
