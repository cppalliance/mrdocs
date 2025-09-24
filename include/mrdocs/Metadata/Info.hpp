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

#ifndef MRDOCS_API_METADATA_INFO_HPP
#define MRDOCS_API_METADATA_INFO_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Info/InfoBase.hpp>

namespace clang::mrdocs {

/** Invoke a function object with a type derived from Info

    This function will invoke the function object `fn` with
    a type derived from `Info` as the first argument, followed
    by `args...`. The type of the first argument is determined
    by the `InfoKind` of the `Info` object.

    @param info The Info object to visit
    @param fn The function object to call
    @param args Additional arguments to pass to the function object
    @return The result of calling the function object with the derived type
 */
template<
    std::derived_from<Info> InfoTy,
    class Fn,
    class... Args>
decltype(auto)
visit(
    InfoTy& info,
    Fn&& fn,
    Args&&... args)
{
    auto visitor = makeVisitor<Info>(
        info, std::forward<Fn>(fn),
        std::forward<Args>(args)...);
    switch(info.Kind)
    {
    #define INFO(Type) \
    case InfoKind::Type: \
        return visitor.template visit<Type##Info>();
#include <mrdocs/Metadata/Info/InfoNodes.inc>
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Merges two Info objects according to the behavior of the derived class.

    @param I The Info object to merge into.
    @param Other The Info object to merge from.
 */
template <polymorphic_storage_for<Info> InfoTy>
void
merge(InfoTy& I, InfoTy&& Other)
{
    MRDOCS_ASSERT(I.Kind == Other.Kind);
    MRDOCS_ASSERT(I.id == Other.id);
    Info& base = I;
    visit(base, [&]<typename DerivedInfoTy>(DerivedInfoTy& derived) mutable
    {
        DerivedInfoTy& otherDerived = static_cast<DerivedInfoTy&>(Other);
        merge(derived, std::move(otherDerived));
    });
}

/** A concept for types that have `Info` members.

    In most cases `T` is another `Info` type that
    has a `Members` member which is a range of
    `SymbolID` values.
*/
template <class InfoTy>
concept InfoParent = requires(InfoTy const& I)
{
    { allMembers(I) } -> range_of<SymbolID>;
};

/** Map the Polymorphic Info to a @ref dom::Object.

    @param io The output parameter to receive the dom::Object.
    @param I The polymorphic Info to convert.
    @param domCorpus The DomCorpus used to resolve references.
 */
template <class IO, polymorphic_storage_for<Info> PolymorphicInfo>
requires std::derived_from<PolymorphicInfo, Info>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    PolymorphicInfo const& I,
    DomCorpus const* domCorpus)
{
    visit(*I, [&](auto const& U)
    {
        tag_invoke(
            dom::LazyObjectMapTag{},
            io,
            U,
            domCorpus);
    });
}

/** Map the Polymorphic Info as a @ref dom::Value object.

    @param io The output parameter to receive the dom::Value.
    @param I The polymorphic Info to convert.
    @param domCorpus The DomCorpus used to resolve references.
 */
template <class IO, polymorphic_storage_for<Info> InfoTy>
requires std::derived_from<InfoTy, Info>
void
tag_invoke(
    dom::ValueFromTag,
    IO& io,
    InfoTy const& I,
    DomCorpus const* domCorpus)
{
    visit(*I, [&](auto const& U)
    {
        tag_invoke(
            dom::ValueFromTag{},
            io,
            U,
            domCorpus);
    });
}

} // clang::mrdocs

#endif
