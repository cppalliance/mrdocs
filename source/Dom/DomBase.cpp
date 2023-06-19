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
#include <mrdox/Dom/DomSymbol.hpp>
#include <mrdox/Dom/DomType.hpp>

namespace clang {
namespace mrdox {

DomBase::
DomBase(
    BaseInfo const& B,
    Corpus const& corpus) noexcept
    : B_(B)
    , corpus_(corpus)
{
}

dom::Value
DomBase::
get(std::string_view key) const
{
    if(key == "type")
        return dom::create<DomType>(B_.Type, corpus_);
    if(key == "access")
        return toString(B_.Access);
    if(key == "isVirtual")
        return B_.IsVirtual;
    return nullptr;
}

auto
DomBase::
props() const ->
    std::vector<std::string_view>
{
    return {
        "type",
        "access",
        "isVirtual"
        };
}

} // mrdox
} // clang
