//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Dom/DomBase.hpp>
#include <mrdox/Dom/DomBaseArray.hpp>

namespace clang {
namespace mrdox {

DomBaseArray::
DomBaseArray(
    std::vector<BaseInfo> const& list,
    Corpus const& corpus) noexcept
    : list_(list)
    , corpus_(corpus)
{
}

std::size_t
DomBaseArray::
length() const noexcept
{
    return list_.size();
}

dom::Value
DomBaseArray::
get(
    std::size_t index) const
{
    if(index < list_.size())
        return dom::create<DomBase>(
            list_[index], corpus_);
    return nullptr;
}

} // mrdox
} // clang
