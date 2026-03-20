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
#include <mrdocs/Support/Describe.hpp>

namespace mrdocs {

/** Represents a set of function overloads.
*/
struct OverloadsSymbol final
    : SymbolCommonBase<SymbolKind::Overloads>
{
    /// The class of the functions.
    FunctionClass FuncClass = FunctionClass::Normal;

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

    /** Create an empty overload set for the given ID.
    */
    explicit OverloadsSymbol(SymbolID const& ID) noexcept
    : SymbolCommonBase(ID)
    {
    }

    /** Create an overload set under the given parent and name.
        @param Parent Owning symbol ID.
        @param Name Unqualified name shared by the overloads.
        @param Access Access specifier when the overloads are members.
        @param isStatic Whether the overload set refers to static functions.
    */
    explicit
    OverloadsSymbol(SymbolID const& Parent, std::string_view Name, AccessKind Access, bool isStatic) noexcept;
};

MRDOCS_DESCRIBE_STRUCT(
    OverloadsSymbol,
    (Symbol),
    (FuncClass, OverloadedOperator, Members, ReturnType)
)

/** Merge overload sets, preserving ordering in `Members`.
*/
MRDOCS_DECL
void merge(OverloadsSymbol& I, OverloadsSymbol&& Other);

/** Access the list of overload members.
    @return Reference to the ID list backing this set.
*/
inline
auto&
allMembers(OverloadsSymbol const& T)
{
    return T.Members;
}

/** Append a new function overload to the set.
*/
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
    DomCorpus const* domCorpus);

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
