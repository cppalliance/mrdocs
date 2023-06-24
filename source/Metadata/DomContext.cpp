//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Metadata/DomContext.hpp>
#include <utility>

namespace clang {
namespace mrdox {

DomContext::
DomContext(
    Hash hash)
    : hash_(std::move(hash))
{
}

dom::Value
DomContext::
get(std::string_view key) const
{
    auto it = hash_.find(key);
    if(it != hash_.end())
        return it->second;
    return nullptr;
}

auto
DomContext::
props() const ->
    std::vector<std::string_view>
{
    std::vector<std::string_view> list;
    list.reserve(hash_.size());
    for(auto const& v : hash_)
        list.push_back(v.first);
    return list;
}

} // mrdox
} // clang
