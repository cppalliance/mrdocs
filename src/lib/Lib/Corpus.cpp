//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "lib/Lib/ConfigImpl.hpp"
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Support/Error.hpp>
#include <ranges>

namespace clang::mrdocs {

//------------------------------------------------

Corpus::~Corpus() noexcept = default;

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
NamespaceInfo const&
Corpus::
globalNamespace() const noexcept
{
    return get<NamespaceInfo>(SymbolID::global);
}

//------------------------------------------------
//
// Modifiers
//
//------------------------------------------------

std::vector<SymbolID>
getParents(Corpus const& C, Info const& I)
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

} // clang::mrdocs
