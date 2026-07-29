//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TARG_HPP
#define MRDOCS_API_METADATA_TARG_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/TArg/ConstantTArg.hpp>
#include <mrdocs/Metadata/TArg/TArgBase.hpp>
#include <mrdocs/Metadata/TArg/TemplateTArg.hpp>
#include <mrdocs/Metadata/TArg/TypeTArg.hpp>
#include <mrdocs/Support/TypeTraits/Visitor.hpp>

namespace mrdocs {

/** Visit a template argument, dispatching on its concrete kind.
*/
template<
    std::derived_from<TArg> TArgTy,
    class F,
    class... Args>
constexpr
decltype(auto)
visit(
    TArgTy& A,
    F&& f,
    Args&&... args)
{
    auto visitor = makeVisitor<TArg>(
        A, std::forward<F>(f), std::forward<Args>(args)...);
    switch(A.Kind)
    {
    #define INFO(Type) case TArgKind::Type: \
        return visitor.template visit<Type##TArg>();
#include <mrdocs/Metadata/TArg/TArgInfoNodes.inc>
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Compare polymorphic template arguments.
*/
MRDOCS_DECL
std::strong_ordering
operator<=>(Polymorphic<TArg> const& lhs, Polymorphic<TArg> const& rhs);

/** Equality for polymorphic template arguments.
*/
inline bool
operator==(Polymorphic<TArg> const& a, Polymorphic<TArg> const& b)
{
    return std::is_eq(a <=> b);
}

/** Serialize a polymorphic template argument into a DOM value.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Polymorphic<TArg> const& I,
    DomCorpus const* domCorpus)
{
    MRDOCS_ASSERT(!I.valueless_after_move());
    tag_invoke(dom::ValueFromTag{}, v, *I, domCorpus);
}

} // mrdocs

#endif
