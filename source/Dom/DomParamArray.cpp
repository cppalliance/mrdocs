//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Dom/DomParam.hpp>
#include <mrdox/Dom/DomParamArray.hpp>

namespace clang {
namespace mrdox {

DomParamArray::
DomParamArray(
    std::vector<Param> const& list,
    Corpus const& corpus) noexcept
    : list_(list)
    , corpus_(corpus)
{
}

std::size_t
DomParamArray::
length() const noexcept
{
    return list_.size();
}

dom::Value
DomParamArray::
get(std::size_t index) const
{
    if(index >= list_.size())
        return nullptr;
    return dom::makePointer<DomParam>(
        list_[index], corpus_);
}

} // mrdox
} // clang
