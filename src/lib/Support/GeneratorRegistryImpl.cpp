//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "GeneratorRegistryImpl.hpp"
#include <mrdocs/Support/Report.hpp>
#include <llvm/ADT/STLExtras.h>


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

extern
std::unique_ptr<Generator>
makeNoopGenerator();

GeneratorRegistry::
~GeneratorRegistry() noexcept = default;

void
GeneratorRegistryImpl::
refresh_plist()
{
    plist_.clear();
    plist_.reserve(list_.size());
    for(auto const& g : list_)
        plist_.push_back(g.get());
}

GeneratorRegistryImpl::
GeneratorRegistryImpl()
{
    insert(makeAdocGenerator());
    insert(makeXMLGenerator());
    insert(makeHTMLGenerator());
    insert(makeNoopGenerator());
}

Generator const*
GeneratorRegistryImpl::
find(
    std::string_view id) const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
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
GeneratorRegistryImpl::
insert(
    std::unique_ptr<Generator> G)
{
    if (!G)
    {
        return Unexpected(formatError("cannot install null generator"));
    }
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto const& li : list_)
    {
        if (li->id() == G->id())
        {
            return Unexpected(formatError("generator id=\"{}\" already exists", G->id()));
        }
    }
    list_.emplace_back(std::move(G));
    refresh_plist();
    return {};
}

//------------------------------------------------

GeneratorRegistryImpl&
getGeneratorRegistryImpl() noexcept
{
    static GeneratorRegistryImpl impl;
    return impl;
}

} // mrdocs
