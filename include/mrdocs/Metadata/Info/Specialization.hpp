//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SPECIALIZATION_HPP
#define MRDOCS_API_METADATA_SPECIALIZATION_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Template.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <vector>

namespace clang::mrdocs {

/** Specialization info for members of implicit instantiations
*/
struct SpecializationInfo final
    : InfoCommonBase<InfoKind::Specialization>
{
    /** The template arguments the parent template is specialized for */
    std::vector<Polymorphic<TArg>> Args;

    /** ID of the template to which the arguments pertain */
    SymbolID Primary = SymbolID::invalid;

    explicit SpecializationInfo(SymbolID ID) noexcept
        : InfoCommonBase(ID)
    {
    }
};

MRDOCS_DECL
void
merge(SpecializationInfo& I, SpecializationInfo&& Other);

/** Map a SpecializationInfo to a dom::Object.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    SpecializationInfo const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Info const&>(I), domCorpus);
}

/** Map the SpecializationInfo to a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    SpecializationInfo const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}


} // clang::mrdocs

#endif
