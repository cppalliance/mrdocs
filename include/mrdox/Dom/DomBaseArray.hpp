//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_DOM_DOMBASEARRAY_HPP
#define MRDOX_API_DOM_DOMBASEARRAY_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/Support/Dom.hpp>
#include <vector>

namespace clang {
namespace mrdox {

/** A vector of BaseInfo.
*/
class MRDOX_DECL
    DomBaseArray : public dom::Array
{
    std::vector<BaseInfo> const& list_;
    Corpus const& corpus_;

public:
    DomBaseArray(
        std::vector<BaseInfo> const& list,
        Corpus const& corpus) noexcept;

    std::size_t length() const noexcept override;
    dom::Value get(std::size_t index) const override;
};

} // mrdox
} // clang

#endif
