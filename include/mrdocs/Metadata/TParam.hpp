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

namespace clang::mrdocs {

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
    switch(P.Kind)
    {
    case TParamKind::Type:
        return f(static_cast<add_cv_from_t<
            TParamTy, TypeTParam>&>(P),
            std::forward<Args>(args)...);
    case TParamKind::Constant:
        return f(static_cast<add_cv_from_t<
            TParamTy, ConstantTParam>&>(P),
            std::forward<Args>(args)...);
    case TParamKind::Template:
        return f(static_cast<add_cv_from_t<
            TParamTy, TemplateTParam>&>(P),
            std::forward<Args>(args)...);
    default:
        MRDOCS_UNREACHABLE();
    }
}

MRDOCS_DECL
std::strong_ordering
operator<=>(Polymorphic<TParam> const& lhs, Polymorphic<TParam> const& rhs);

inline
bool
operator==(Polymorphic<TParam> const& lhs, Polymorphic<TParam> const& rhs) {
    return lhs <=> rhs == std::strong_ordering::equal;
}

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


} // clang::mrdocs

#endif
