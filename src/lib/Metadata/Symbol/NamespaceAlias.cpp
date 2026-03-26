//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Support/Reflection.hpp>
#include <mrdocs/Metadata/Symbol/NamespaceAlias.hpp>

namespace mrdocs {

template <typename IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    NamespaceAliasSymbol const& I,
    DomCorpus const* domCorpus)
{
    mapReflectedType<true>(io, I, domCorpus);
}

template
void
tag_invoke<LazyObjectIOType>(
    dom::LazyObjectMapTag,
    LazyObjectIOType&,
    NamespaceAliasSymbol const&,
    DomCorpus const*);

} // mrdocs
