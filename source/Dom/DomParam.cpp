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
#include <mrdox/Dom/DomType.hpp>

namespace clang {
namespace mrdox {

DomParam::
DomParam(
    Param const& I,
    Corpus const& corpus) noexcept
    : I_(&I)
    , corpus_(corpus)
{
}

dom::Value
DomParam::
get(std::string_view key) const
{
    if(key == "name")
        return dom::nonEmptyString(I_->Name);
    if(key == "type")
        return dom::makePointer<DomType>(I_->Type, corpus_);
    if(key == "default")
        return dom::nonEmptyString(I_->Default);
    return nullptr;
}

auto
DomParam::
props() const ->
    std::vector<std::string_view>
{
    return {
        "name",
        "type",
        "default"
    };
}

} // mrdox
} // clang
