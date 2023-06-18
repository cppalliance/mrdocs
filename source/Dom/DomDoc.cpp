//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Dom/DomDoc.hpp>

namespace clang {
namespace mrdox {

DomDoc::
DomDoc(
    Corpus const& corpus) noexcept
    : corpus_(corpus)
{
}

dom::Value
DomDoc::
get(std::string_view key) const
{
    return nullptr;
}

auto
DomDoc::
props() const ->
    std::vector<std::string_view> 
{
    return {};
};

} // mrdox
} // clang
