//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Metadata/Record.hpp>
#include <lib/Dom/LazyObject.hpp>

namespace clang {
namespace mrdocs {

dom::String
toString(
    RecordKeyKind kind) noexcept
{
    switch(kind)
    {
    case RecordKeyKind::Struct:
        return "struct";
    case RecordKeyKind::Class:
        return "class";
    case RecordKeyKind::Union:
        return "union";
    default:
        MRDOCS_UNREACHABLE();
    }
}

template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    BaseInfo const& I,
    DomCorpus const* domCorpus)
{
    io.map("access", I.Access);
    io.map("isVirtual", I.IsVirtual);
    io.map("type", dom::ValueFrom(I.Type, domCorpus));
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
} // clang
