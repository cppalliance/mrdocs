//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <lib/CompilationDatabaseBuilder.hpp>
#include <lib/ConfigImpl.hpp>
#include <lib/CorpusImpl.hpp>
#include <lib/Support/Path.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Support/Error.hpp>
#include <algorithm>
#include <compare>
#include <ranges>

namespace mrdocs {

//------------------------------------------------

Corpus::~Corpus() noexcept = default;

Expected<std::unique_ptr<Corpus>>
Corpus::
build(std::shared_ptr<Config const> const& config)
{
    MRDOCS_CHECK(config, "config must not be null");
    auto const configImpl =
        std::dynamic_pointer_cast<ConfigImpl const>(config);
    MRDOCS_CHECK(configImpl,
        "config was not produced by Config::load");

    MRDOCS_TRY(
        MrDocsCompilationDatabase compilations,
        generateCompilationDatabase(configImpl));

    return CorpusImpl::build(configImpl, compilations);
}

//------------------------------------------------
//
// Observers
//
//------------------------------------------------

bool
Corpus::
empty() const noexcept
{
    return begin() == end();
}

/** Return the metadata for the global namespace.
*/
NamespaceSymbol const&
Corpus::
globalNamespace() const noexcept
{
    return get<NamespaceSymbol>(SymbolID::global);
}

//------------------------------------------------
//
// Modifiers
//
//------------------------------------------------

std::vector<SymbolID>
getParents(Corpus const& C, Symbol const& I)
{
    std::vector<SymbolID> parents;
    std::size_t n = 0;
    auto curParent = I.Parent;
    while (curParent)
    {
        ++n;
        MRDOCS_ASSERT(C.find(curParent));
        curParent = C.get(curParent).Parent;
    }
    parents.reserve(n);
    parents.resize(n);
    curParent = I.Parent;
    while (curParent)
    {
        parents[--n] = curParent;
        MRDOCS_ASSERT(C.find(curParent));
        curParent = C.get(curParent).Parent;
    }
    return parents;
}

} // mrdocs
