//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_NAME_HPP
#define MRDOCS_API_METADATA_NAME_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Name/IdentifierName.hpp>
#include <mrdocs/Metadata/Name/NameBase.hpp>
#include <mrdocs/Metadata/Name/SpecializationName.hpp>
#include <mrdocs/Support/Visitor.hpp>

namespace mrdocs {

template<
    std::derived_from<Name> NameTy,
    class Fn,
    class... Args>
decltype(auto)
visit(
    NameTy& info,
    Fn&& fn,
    Args&&... args)
{
    auto visitor = makeVisitor<Name>(
        info, std::forward<Fn>(fn), std::forward<Args>(args)...);
    switch(info.Kind)
    {
    #define INFO(Type) case NameKind::Type: \
        return visitor.template visit<Type##Name>();
#include <mrdocs/Metadata/Name/NameNodes.inc>
    default:
        MRDOCS_UNREACHABLE();
    }
}

MRDOCS_DECL
std::strong_ordering
operator<=>(Polymorphic<Name> const& lhs, Polymorphic<Name> const& rhs);

inline bool
operator==(Polymorphic<Name> const& lhs, Polymorphic<Name> const& rhs)
{
    return lhs <=> rhs == std::strong_ordering::equal;
}

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Polymorphic<Name> const& I,
    DomCorpus const* domCorpus)
{
    MRDOCS_ASSERT(!I.valueless_after_move());
    tag_invoke(dom::ValueFromTag{}, v, *I, domCorpus);
}

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Optional<Polymorphic<Name>> const& I,
    DomCorpus const* domCorpus)
{
    if (!I)
    {
        v = nullptr;
        return;
    }
    MRDOCS_ASSERT(!I->valueless_after_move());
    tag_invoke(dom::ValueFromTag{}, v, *I, domCorpus);
}

} // mrdocs

#endif
