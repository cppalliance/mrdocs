//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SYMBOL_OVERLOADS_HPP
#define MRDOCS_API_METADATA_SYMBOL_OVERLOADS_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Symbol.hpp>
#include <mrdocs/Metadata/Symbol/Function.hpp>

namespace mrdocs {

/** Represents a set of function overloads.
 */
struct OverloadsSymbol final
    : SymbolCommonBase<SymbolKind::Overloads>
{
    /// The class of the functions.
    FunctionClass Class = FunctionClass::Normal;

    /// The overloaded operator, if any.
    OperatorKind OverloadedOperator = OperatorKind::None;

    /// The members of the overload set.
    std::vector<SymbolID> Members;

    /** Info about the return type of these function overloads.

        If all overloads have the same return type, this contains
        that type. Otherwise, it contains `auto` to indicate that
        the return type varies according to the parameters.
     */
    Polymorphic<Type> ReturnType = Polymorphic<Type>(AutoType{});

    //--------------------------------------------

    explicit OverloadsSymbol(SymbolID const& ID) noexcept
    : SymbolCommonBase(ID)
    {
    }

    explicit
    OverloadsSymbol(SymbolID const& Parent, std::string_view Name, AccessKind Access, bool isStatic) noexcept;
};

MRDOCS_DECL
void merge(OverloadsSymbol& I, OverloadsSymbol&& Other);

inline
auto&
allMembers(OverloadsSymbol const& T)
{
    return T.Members;
}

MRDOCS_DECL
void
addMember(OverloadsSymbol& I, FunctionSymbol const& Member);

/** Map a OverloadsSymbol to a dom::Object.

    @param t The tag type.
    @param io The IO object to use for mapping.
    @param I The OverloadsSymbol to map.
    @param domCorpus The DomCorpus used to create
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    OverloadsSymbol const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, I.asInfo(), domCorpus);
    io.map("class", I.Class);
    io.map("overloadedOperator", I.OverloadedOperator);
    io.map("members", dom::LazyArray(I.Members, domCorpus));
}

/** Map the OverloadsSymbol to a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    OverloadsSymbol const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_OVERLOADS_HPP
