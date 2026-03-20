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

#include <lib/Support/Reflection/MergeReflectedType.hpp>
#include <lib/Support/Reflection/MapReflectedType.hpp>
#include <mrdocs/Dom/LazyObject.hpp>
#include <mrdocs/Metadata/Name.hpp>
#include <mrdocs/Metadata/Symbol/Record.hpp>

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

void
merge(RecordTranche& I, RecordTranche&& Other)
{
    mergeReflected(I, Other);
}

void
merge(RecordInterface& I, RecordInterface&& Other)
{
    mergeReflected(I, Other);
}

void
merge(RecordSymbol& I, RecordSymbol&& Other)
{
    mergeReflected(I, Other);
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

} // mrdocs
