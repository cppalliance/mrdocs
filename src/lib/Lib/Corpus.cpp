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

namespace clang {
namespace mrdocs {

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

// KRYSTIAN NOTE: temporary
std::string&
Corpus::
getFullyQualifiedName(
    const Info& I,
    std::string& temp) const
{
    temp.clear();
    if(! I.id || I.id == SymbolID::global)
        return temp;

    MRDOCS_ASSERT(I.Parent);
    for(auto const& pID : getParents(*this, I))
    {
        if (!pID || pID == SymbolID::global)
        {
            continue;
        }
        if (Info const* P = find(pID))
        {
            if (!P->Name.empty())
            {
                temp.append(P->Name);
            }
            else
            {
                fmt::format_to(
                    std::back_inserter(temp),
                    "<unnamed {}>",
                    toString(P->Kind));
            }
        }
        else
        {
            temp.append("<unnamed>");
        }
        temp.append("::");
    }
    if (I.Name.empty())
    {
        fmt::format_to(
            std::back_inserter(temp),
            "<unnamed {}>",
            toString(I.Kind));
    }
    else
    {
        temp.append(I.Name);
    }
    return temp;
}

std::vector<SymbolID>
getParents(Corpus const& C, const Info& I)
{
    // AFREITAS: This function could eventually
    // return a view.
    std::vector<SymbolID> parents;
    auto curParent = I.Parent;
    while (curParent)
    {
        parents.push_back(curParent);
        curParent = C.get(curParent).Parent;
    }
    std::ranges::reverse(parents);
    return parents;
}

} // mrdocs
} // clang
