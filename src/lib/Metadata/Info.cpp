//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2025 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <lib/Support/Radix.hpp>
#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/Dom/LazyObject.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Metadata/Symbol/Record.hpp>
#include <mrdocs/Support/Reflection.hpp>
#include <mrdocs/Support/String.hpp>
#include <clang/AST/Type.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

namespace mrdocs {

dom::String
toString(SymbolKind const kind) noexcept
{
    switch(kind)
    {
#define INFO(Type) \
    case SymbolKind::Type: return toKebabCase(#Type);
#include <mrdocs/Metadata/Symbol/SymbolNodes.inc>
    default:
        MRDOCS_UNREACHABLE();
    }
}

template <typename IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    Symbol const& I,
    DomCorpus const* domCorpus)
{
    MRDOCS_ASSERT(domCorpus);
    mapReflectedType<false>(io, I, domCorpus);
    io.map("class", std::string("symbol"));
    io.map("isRegular", I.Extraction == ExtractionMode::Regular);
    io.map("isSeeBelow", I.Extraction == ExtractionMode::SeeBelow);
    io.map("isImplementationDefined", I.Extraction == ExtractionMode::ImplementationDefined);
    io.map("isDependency", I.Extraction == ExtractionMode::Dependency);
}

template
void
tag_invoke<LazyObjectIOType>(
    dom::LazyObjectMapTag,
    LazyObjectIOType&,
    Symbol const&,
    DomCorpus const*);

inline void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Optional<DocComment> const& opt,
    DomCorpus const* domCorpus)
{
    if (opt)
    {
        v = dom::LazyObject(*opt, domCorpus);
    }
}

} // mrdocs
