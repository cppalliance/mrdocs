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

#ifndef MRDOCS_API_METADATA_INFO_ENUM_HPP
#define MRDOCS_API_METADATA_INFO_ENUM_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Info/Source.hpp>
#include <mrdocs/Metadata/Type.hpp>

namespace clang::mrdocs {

struct EnumInfo final
    : InfoCommonBase<InfoKind::Enum>
{
    /** Indicates whether this enum is scoped (e.g. enum class).

        If true, the enumerators are accessed with the scope resolution
        operator (e.g. EnumName::Enumerator).

        If false, the enumerators are accessed directly (e.g. Enumerator)
        in the parent context.
    */
    bool Scoped = false;

    /** The underlying type of this enum, if explicitly specified.

        If not specified, the underlying type is an implementation-defined
        integral type that can represent all the enumerator values defined in
        the enumeration.

        For `enum Foo : short { ... };` this will be represent `short`.
    */
    Optional<Polymorphic<TypeInfo>> UnderlyingType = std::nullopt;

    /** The members of this scope.

        All members are enum constants.

        Enum constants are independent symbol types that
        can be documented separately.
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

    @param t The tag type.
    @param io The IO object to use for mapping.
    @param I The EnumInfo to map.
    @param domCorpus The DomCorpus used to create
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

    @param v The output parameter to receive the dom::Value.
    @param I The EnumInfo to convert.
    @param domCorpus The DomCorpus used to resolve references.
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

#endif // MRDOCS_API_METADATA_INFO_ENUM_HPP
