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
#include <mrdocs/Metadata/Name/IdentifierNameInfo.hpp>
#include <mrdocs/Metadata/Name/NameBase.hpp>
#include <mrdocs/Metadata/Name/SpecializationNameInfo.hpp>
#include <mrdocs/Support/Visitor.hpp>

namespace clang::mrdocs {

template<
    std::derived_from<NameInfo> NameInfoTy,
    class Fn,
    class... Args>
decltype(auto)
visit(
    NameInfoTy& info,
    Fn&& fn,
    Args&&... args)
{
    auto visitor = makeVisitor<NameInfo>(
        info, std::forward<Fn>(fn), std::forward<Args>(args)...);
    switch(info.Kind)
    {
    case NameKind::Identifier:
        return visitor.template visit<IdentifierNameInfo>();
    case NameKind::Specialization:
        return visitor.template visit<SpecializationNameInfo>();
    default:
        MRDOCS_UNREACHABLE();
    }
}

MRDOCS_DECL
std::strong_ordering
operator<=>(Polymorphic<NameInfo> const& lhs, Polymorphic<NameInfo> const& rhs);

inline bool
operator==(Polymorphic<NameInfo> const& lhs, Polymorphic<NameInfo> const& rhs)
{
    return lhs <=> rhs == std::strong_ordering::equal;
}

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Polymorphic<NameInfo> const& I,
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
    Optional<Polymorphic<NameInfo>> const& I,
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

} // clang::mrdocs

#endif
