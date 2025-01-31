//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_ENUM_HPP
#define MRDOCS_API_METADATA_ENUM_HPP

#include <mrdocs/ADT/PolymorphicValue.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Source.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <mrdocs/Dom/LazyArray.hpp>
#include <ranges>

namespace clang::mrdocs {

struct EnumInfo final
    : InfoCommonBase<InfoKind::Enum>
{
    // Indicates whether this enum is scoped (e.g. enum class).
    bool Scoped = false;

    // Set too nonempty to the type when this is an explicitly typed enum. For
    //   enum Foo : short { ... };
    // this will be "short".
    PolymorphicValue<TypeInfo> UnderlyingType;

    /** The members of this scope.

        All members are enum constants;
    */
    std::vector<SymbolID> Constants;

    //--------------------------------------------

    explicit EnumInfo(SymbolID ID) noexcept
        : InfoCommonBase(ID)
    {
    }
};

inline
auto&
allMembers(EnumInfo const& T)
{
    return T.Constants;
}

MRDOCS_DECL
void
merge(EnumInfo& I, EnumInfo&& Other);

/** Map a EnumInfo to a dom::Object.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    EnumInfo const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Info const&>(I), domCorpus);
    io.map("type", I.UnderlyingType);
    io.map("isScoped", I.Scoped);
    io.map("constants", dom::LazyArray(I.Constants, domCorpus));
}

/** Map the EnumInfo to a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    EnumInfo const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // clang::mrdocs

#endif
