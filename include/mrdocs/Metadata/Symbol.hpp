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

#ifndef MRDOCS_API_METADATA_SYMBOL_HPP
#define MRDOCS_API_METADATA_SYMBOL_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Symbol/SymbolBase.hpp>

namespace mrdocs {

/** Invoke a function object with a type derived from Symbol

    This function will invoke the function object `fn` with
    a type derived from `Symbol` as the first argument, followed
    by `args...`. The type of the first argument is determined
    by the `SymbolKind` of the `Symbol` object.

    @param info The Symbol object to visit
    @param fn The function object to call
    @param args Additional arguments to pass to the function object
    @return The result of calling the function object with the derived type
 */
template<
    std::derived_from<Symbol> SymbolTy,
    class Fn,
    class... Args>
decltype(auto)
visit(
    SymbolTy& info,
    Fn&& fn,
    Args&&... args)
{
    auto visitor = makeVisitor<
        Symbol>(
        info, std::forward<Fn>(fn),
        std::forward<Args>(args)...);
    switch(info.Kind)
    {
    #define INFO(Type) \
    case SymbolKind::Type: \
        return visitor.template visit<Type##Symbol>();
#include <mrdocs/Metadata/Symbol/SymbolNodes.inc>
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Merges two Symbol objects according to the behavior of the derived class.

    @param I The Symbol object to merge into.
    @param Other The Symbol object to merge from.
 */
template <polymorphic_storage_for<Symbol> SymbolTy>
void
merge(SymbolTy& I, SymbolTy&& Other)
{
    MRDOCS_ASSERT(I.Kind == Other.Kind);
    MRDOCS_ASSERT(I.id == Other.id);
    Symbol& base = I;
    visit(base, [&]<typename DerivedSymbolTy>(DerivedSymbolTy& derived) mutable
    {
        DerivedSymbolTy& otherDerived = static_cast<DerivedSymbolTy&>(Other);
        merge(derived, std::move(otherDerived));
    });
}

/** A concept for types that have `Symbol` members.

    In most cases `T` is another `Symbol` type that
    has a `Members` member which is a range of
    `SymbolID` values.
*/
template <class SymbolTy>
concept SymbolParent = requires(SymbolTy const& I)
{
    { allMembers(I) } -> range_of<SymbolID>;
};

/** Map the Polymorphic Symbol to a @ref dom::Object.

    @param io The output parameter to receive the dom::Object.
    @param I The polymorphic Symbol to convert.
    @param domCorpus The DomCorpus used to resolve references.
 */
template <class IO, polymorphic_storage_for<Symbol> PolymorphicSymbol>
requires std::derived_from<PolymorphicSymbol, Symbol>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    PolymorphicSymbol const& I,
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

/** Map the Polymorphic Symbol as a @ref dom::Value object.

    @param io The output parameter to receive the dom::Value.
    @param I The polymorphic Symbol to convert.
    @param domCorpus The DomCorpus used to resolve references.
 */
//template <class IO, polymorphic_storage_for<Symbol> SymbolTy>
//requires std::derived_from<SymbolTy, Symbol>
//void
//tag_invoke(
//    dom::ValueFromTag,
//    IO& io,
//    SymbolTy const& I,
//    DomCorpus const* domCorpus)
//{
//    visit(*I, [&](auto const& U)
//    {
//        tag_invoke(
//            dom::ValueFromTag{},
//            io,
//            U,
//            domCorpus);
//    });
//}

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_HPP
