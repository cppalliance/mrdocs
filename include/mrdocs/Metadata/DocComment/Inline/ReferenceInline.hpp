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

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_INLINE_REFERENCEINLINE_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_INLINE_REFERENCEINLINE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/DocComment/Inline/TextInline.hpp>
#include <mrdocs/Metadata/Symbol/SymbolID.hpp>
#include <string>

namespace mrdocs::doc {

/** A reference to a symbol.
*/
struct ReferenceInline
    : InlineCommonBase<InlineKind::Reference>
{
    std::string literal;
    SymbolID id = SymbolID::invalid;

    explicit ReferenceInline(std::string str = {}) noexcept
        : literal(std::move(str))
    {}

    auto operator<=>(ReferenceInline const&) const = default;
    bool operator==(ReferenceInline const&) const noexcept = default;
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
    ReferenceInline const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Inline const&>(I), domCorpus);
    io.map("literal", I.literal);
    io.map("symbol", I.id);
}

/** Return the @ref Reference as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    ReferenceInline const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_REFERENCEINLINE_HPP
