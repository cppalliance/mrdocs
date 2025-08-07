//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_OVERLOADS_HPP
#define MRDOCS_API_METADATA_OVERLOADS_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Info/Function.hpp>

namespace clang::mrdocs {

/** Represents a set of function overloads.
 */
struct OverloadsInfo final
    : InfoCommonBase<InfoKind::Overloads>
{
    /// The class of the functions.
    FunctionClass Class = FunctionClass::Normal;

    /// The overloaded operator, if any.
    OperatorKind OverloadedOperator = OperatorKind::None;

    /// The members of the overload set.
    std::vector<SymbolID> Members;

    /// Info about the return type of this function.
    Polymorphic<TypeInfo> ReturnType = std::nullopt;

    //--------------------------------------------

    explicit OverloadsInfo(SymbolID const& ID) noexcept
    : InfoCommonBase(ID)
    {
    }

    explicit
    OverloadsInfo(SymbolID const& Parent, std::string_view Name, AccessKind Access, bool isStatic) noexcept;
};

MRDOCS_DECL
void merge(OverloadsInfo& I, OverloadsInfo&& Other);

inline
auto&
allMembers(OverloadsInfo const& T)
{
    return T.Members;
}

MRDOCS_DECL
void
addMember(OverloadsInfo& I, FunctionInfo const& Member);

/** Map a OverloadsInfo to a dom::Object.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    OverloadsInfo const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Info const&>(I), domCorpus);
    io.map("class", I.Class);
    io.map("overloadedOperator", I.OverloadedOperator);
    io.map("members", dom::LazyArray(I.Members, domCorpus));
}

/** Map the OverloadsInfo to a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    OverloadsInfo const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // clang::mrdocs

#endif
