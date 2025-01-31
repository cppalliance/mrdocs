//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "GeneratorsImpl.hpp"
#include <llvm/ADT/STLExtras.h>

namespace clang {
namespace mrdocs {

extern
std::unique_ptr<Generator>
makeAdocGenerator();

extern
std::unique_ptr<Generator>
makeXMLGenerator();

extern
std::unique_ptr<Generator>
makeHTMLGenerator();

Generators::
~Generators() noexcept = default;

void
GeneratorsImpl::
refresh_plist()
{
    plist_.clear();
    plist_.reserve(list_.size());
    for(auto const& g : list_)
        plist_.push_back(g.get());
}

GeneratorsImpl::
GeneratorsImpl()
{
    insert(makeAdocGenerator());
    insert(makeXMLGenerator());
    insert(makeHTMLGenerator());
}

Generator const*
GeneratorsImpl::
find(
    std::string_view id) const noexcept
{
    for (auto const& li : list_)
    {
        if(li->id() == id)
        {
            return li.get();
        }
    }
    return nullptr;
}

Expected<void>
GeneratorsImpl::
insert(
    std::unique_ptr<Generator> G)
{
    MRDOCS_CHECK(find(G->id()) == nullptr, formatError("generator id=\"{}\" already exists", G->id()));
    list_.emplace_back(std::move(G));
    refresh_plist();
    return {};
}

//------------------------------------------------

GeneratorsImpl&
getGeneratorsImpl() noexcept
{
    static GeneratorsImpl impl;
    return impl;
}

Generators const&
getGenerators() noexcept
{
    return getGeneratorsImpl();
}

} // mrdocs
} // clang
