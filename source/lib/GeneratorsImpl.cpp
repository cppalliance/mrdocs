//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "GeneratorsImpl.hpp"
#include <llvm/ADT/STLExtras.h>

namespace clang {
namespace mrdox {

extern
std::unique_ptr<Generator>
makeXMLGenerator();

extern
std::unique_ptr<Generator>
makeAsciidocGenerator();

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
    llvm::handleAllErrors(insert(makeAsciidocGenerator()));
    llvm::handleAllErrors(insert(makeXMLGenerator()));
}

Generator const*
GeneratorsImpl::
find(
    llvm::StringRef name) const noexcept
{
    for(std::size_t i = 0; i < list_.size(); ++i)
        if(list_[i]->name() == name)
            return list_[i].get();
    return nullptr;
}

llvm::Error
GeneratorsImpl::
insert(
    std::unique_ptr<Generator> G)
{
    if(auto g = find(G->name()))
        return makeError("generator '", G->name(), "' already exists");
    list_.emplace_back(std::move(G));
    refresh_plist();
    return llvm::Error::success();
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

} // mrdox
} // clang
