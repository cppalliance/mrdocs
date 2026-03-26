//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Metadata/Symbol/Namespace.hpp>
#include <mrdocs/Support/Reflection.hpp>
#include <llvm/ADT/STLExtras.h>

namespace mrdocs {

std::strong_ordering
NamespaceSymbol::
operator<=>(NamespaceSymbol const& other) const
{
    if (auto const res = this->asInfo() <=> other.asInfo();
        std::is_neq(res))
    {
        return res;
    }
    if (auto const res = IsInline <=> other.IsInline;
        std::is_neq(res))
    {
        return res;
    }
    if (auto const res = IsAnonymous <=> other.IsAnonymous;
        std::is_neq(res))
    {
        return res;
    }
    if (auto const res = UsingDirectives.size() <=> other.UsingDirectives.size();
        std::is_neq(res))
    {
        return res;
    }
    for (auto const& [lhs, rhs] : llvm::zip(UsingDirectives, other.UsingDirectives))
    {
        if (auto const res = lhs <=> rhs;
            std::is_neq(res))
        {
            return res;
        }
    }
    if (auto const res = Members <=> other.Members;
        std::is_neq(res))
    {
        return res;
    }
    return std::strong_ordering::equal;
}


template <typename IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    NamespaceTranche const& I,
    DomCorpus const* domCorpus)
{
    mapReflectedType<true>(io, I, domCorpus);
}

template
void
tag_invoke<LazyObjectIOType>(
    dom::LazyObjectMapTag,
    LazyObjectIOType&,
    NamespaceTranche const&,
    DomCorpus const*);

template <typename IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    NamespaceSymbol const& I,
    DomCorpus const* domCorpus)
{
    mapReflectedType<true>(io, I, domCorpus);
}

template
void
tag_invoke<LazyObjectIOType>(
    dom::LazyObjectMapTag,
    LazyObjectIOType&,
    NamespaceSymbol const&,
    DomCorpus const*);

} // mrdocs

