//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SYMBOL_HPP
#define MRDOCS_API_METADATA_SYMBOL_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Symbol/SymbolBase.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>

namespace mrdocs {

// Register Symbol's concrete kinds so the generic `visit`
// (Support/Reflection/Describe.hpp) can dispatch over them.
#define INFO(Type) MRDOCS_KIND_ENTRY(Symbol, Type##Symbol)
MRDOCS_DESCRIBE_KINDS_BEGIN(Symbol)
#include <mrdocs/Metadata/Symbol/SymbolNodes.inc>
MRDOCS_DESCRIBE_KINDS_END(Symbol)
#undef INFO

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
        auto& otherDerived = static_cast<DerivedSymbolTy&>(Other);
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


} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_HPP
