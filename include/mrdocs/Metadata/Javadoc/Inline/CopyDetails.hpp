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

#ifndef MRDOCS_API_METADATA_JAVADOC_INLINE_COPYDETAILS_HPP
#define MRDOCS_API_METADATA_JAVADOC_INLINE_COPYDETAILS_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Javadoc/Inline/Reference.hpp>
#include <mrdocs/Metadata/Javadoc/Inline/Text.hpp>
#include <string>

namespace mrdocs::doc {

/** Documentation copied from another symbol.
*/
struct CopyDetails final : Reference
{
    static constexpr auto static_kind = NodeKind::copy_details;

    CopyDetails(std::string string_ = std::string()) noexcept
        : Reference(std::move(string_), NodeKind::copy_details)
    {
    }

    auto operator<=>(CopyDetails const&) const = default;
    bool operator==(CopyDetails const&) const noexcept = default;
    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<CopyDetails const&>(other);
    }
};

/** Map the @ref CopyDetails to a @ref dom::Object.

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
    CopyDetails const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Reference const&>(I), domCorpus);
}

/** Return the @ref CopyDetails as a @ref dom::Value object.

    @param v The output value.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    CopyDetails const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_JAVADOC_INLINE_COPYDETAILS_HPP
