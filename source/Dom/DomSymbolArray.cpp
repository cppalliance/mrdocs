//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Dom/DomSymbolArray.hpp>
#include <mrdox/Dom/DomSymbol.hpp>

namespace clang {
namespace mrdox {

DomSymbolArray::
DomSymbolArray(
    std::vector<SymbolID> const& list,
    Corpus const& corpus) noexcept
    : list_(list)
    , corpus_(corpus)
{
}

std::size_t
DomSymbolArray::
length() const noexcept
{
    return list_.size();
}

dom::Value
DomSymbolArray::
get(std::size_t index) const
{
    if(index >= list_.size())
        return nullptr;
    return visit(
        corpus_.get(list_[index]),
        [&]<class T>(T const& I) ->
            dom::ObjectPtr
        {
            return dom::makePointer<
                DomSymbol<T>>(I, corpus_);
        });
}

} // mrdox
} // clang
