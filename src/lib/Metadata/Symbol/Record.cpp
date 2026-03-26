//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2025 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Dom/LazyObject.hpp>
#include <mrdocs/Metadata/Name.hpp>
#include <mrdocs/Metadata/Symbol/Record.hpp>
#include <mrdocs/Metadata/Symbol/RecordTranche.hpp>
#include <mrdocs/Metadata/Symbol/RecordInterface.hpp>
#include <mrdocs/Support/Reflection.hpp>

namespace mrdocs {

std::strong_ordering
RecordSymbol::
operator<=>(RecordSymbol const& other) const
{
    if (auto const cmp = Name <=> other.Name;
        !std::is_eq(cmp))
    {
        return cmp;
    }
    if (auto const cmp = Template.operator bool() <=> other.Template.operator bool();
        !std::is_eq(cmp))
    {
        return cmp;
    }
    if (Template && other.Template)
    {
        if (auto const cmp = Template->Args.size() <=> other.Template->Args.size();
            !std::is_eq(cmp))
        {
            return cmp;
        }
        if (auto const cmp = Template->Params.size() <=> other.Template->Params.size();
            !std::is_eq(cmp))
        {
            return cmp;
        }
    }
    if (Template && other.Template)
    {
        if (auto const cmp = Template->Args <=> other.Template->Args;
            !std::is_eq(cmp))
        {
            return cmp;
        }
        if (auto const cmp = Template->Params <=> other.Template->Params;
            !std::is_eq(cmp))
        {
            return cmp;
        }
    }
    return this->asInfo() <=> other.asInfo();
}

template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    BaseInfo const& I,
    DomCorpus const* domCorpus)
{
    mapReflectedType<true>(io, I, domCorpus);
    io.map("isPublic", I.Access == AccessKind::Public);
    io.map("isProtected", I.Access == AccessKind::Protected);
    io.map("isPrivate", I.Access == AccessKind::Private);
}

void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    BaseInfo const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}


template <typename IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    RecordInterface const& I,
    DomCorpus const*)
{
    mapReflectedType<true>(io, I);
}

template
void
tag_invoke<LazyObjectIOType>(
    dom::LazyObjectMapTag,
    LazyObjectIOType&,
    RecordInterface const&,
    DomCorpus const*);

template <typename IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    RecordSymbol const& I,
    DomCorpus const* domCorpus)
{
    mapReflectedType<true>(io, I, domCorpus);
    io.map("defaultAccess", getDefaultAccessString(I.KeyKind));
}

template <typename IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    RecordTranche const& I,
    DomCorpus const* domCorpus)
{
    mapReflectedType<true>(io, I, domCorpus);
}

template
void
tag_invoke<LazyObjectIOType>(
    dom::LazyObjectMapTag,
    LazyObjectIOType&,
    RecordTranche const&,
    DomCorpus const*);

template
void
tag_invoke<LazyObjectIOType>(
    dom::LazyObjectMapTag,
    LazyObjectIOType&,
    RecordSymbol const&,
    DomCorpus const*);

} // mrdocs
