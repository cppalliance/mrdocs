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

#ifndef MRDOCS_API_METADATA_TPARAM_HPP
#define MRDOCS_API_METADATA_TPARAM_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/TParam/ConstantTParam.hpp>
#include <mrdocs/Metadata/TParam/TParamBase.hpp>
#include <mrdocs/Metadata/TParam/TemplateTParam.hpp>
#include <mrdocs/Metadata/TParam/TypeTParam.hpp>
#include <mrdocs/Support/Visitor.hpp>

namespace mrdocs {

/** Visit a template parameter, dispatching to its concrete type.
    @param P Parameter to visit.
    @param f Callable to receive the concrete parameter.
    @param args Additional arguments forwarded to the callable.
    @return Whatever the callable returns.
*/
template<
    typename TParamTy,
    typename F,
    typename... Args>
    requires std::derived_from<TParamTy, TParam>
constexpr
decltype(auto)
visit(
    TParamTy& P,
    F&& f,
    Args&&... args)
{
    auto visitor = makeVisitor<TParam>(
        P, std::forward<F>(f), std::forward<Args>(args)...);
    switch(P.Kind)
    {
    #define INFO(Type) case TParamKind::Type: \
        return visitor.template visit<Type##TParam>();
#include <mrdocs/Metadata/TParam/TParamInfoNodes.inc>
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Compare polymorphic template parameters.
*/
MRDOCS_DECL
std::strong_ordering
operator<=>(Polymorphic<TParam> const& lhs, Polymorphic<TParam> const& rhs);

/** Equality helper for polymorphic template parameters.
*/
inline
bool
operator==(Polymorphic<TParam> const& lhs, Polymorphic<TParam> const& rhs) {
    return lhs <=> rhs == std::strong_ordering::equal;
}

/** Serialize a polymorphic template parameter.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Polymorphic<TParam> const& I,
    DomCorpus const* domCorpus)
{
    MRDOCS_ASSERT(!I.valueless_after_move());
    tag_invoke(dom::ValueFromTag{}, v, *I, domCorpus);
}


} // mrdocs

#endif
