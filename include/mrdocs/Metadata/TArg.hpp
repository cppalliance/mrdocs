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
#include <mrdocs/Metadata/TArg/NonTypeTArg.hpp>
#include <mrdocs/Metadata/TArg/TArgBase.hpp>
#include <mrdocs/Metadata/TArg/TemplateTArg.hpp>
#include <mrdocs/Metadata/TArg/TypeTArg.hpp>

namespace clang::mrdocs {

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
    switch(A.Kind)
    {
    case TArgKind::Type:
        return f(static_cast<add_cv_from_t<
            TArgTy, TypeTArg>&>(A),
            std::forward<Args>(args)...);
    case TArgKind::NonType:
        return f(static_cast<add_cv_from_t<
            TArgTy, NonTypeTArg>&>(A),
            std::forward<Args>(args)...);
    case TArgKind::Template:
        return f(static_cast<add_cv_from_t<
            TArgTy, TemplateTArg>&>(A),
            std::forward<Args>(args)...);
    default:
        MRDOCS_UNREACHABLE();
    }
}

MRDOCS_DECL
std::strong_ordering
operator<=>(Polymorphic<TArg> const& lhs, Polymorphic<TArg> const& rhs);

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Polymorphic<TArg> const& I,
    DomCorpus const* domCorpus)
{
    if (I.valueless_after_move())
    {
        v = nullptr;
        return;
    }
    tag_invoke(dom::ValueFromTag{}, v, *I, domCorpus);
}

} // clang::mrdocs

#endif
