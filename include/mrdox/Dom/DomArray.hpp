//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_DOM_DOMARRAY_HPP
#define MRDOX_API_DOM_DOMARRAY_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/Support/Dom.hpp>

namespace clang {
namespace mrdox {

template<class T, class U>
class MRDOX_DECL
    DomArray : public dom::Array
{
    std::vector<T> list_;
    Corpus const& corpus_;

public:
    DomArray(
        std::vector<T> const& list,
        Corpus const& corpus) noexcept
        : list_(list)
        , corpus_(corpus)
    {
    }

    std::size_t
    length() const noexcept override
    {
        return list_.size();
    }

    dom::Value
    get(std::size_t index) const override
    {
        if(index >= list_.size())
            return nullptr;
        return dom::create<U>(list_[index], corpus_);
    }
};

} // mrdox
} // clang

#endif
