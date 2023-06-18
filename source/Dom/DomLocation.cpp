//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Dom/DomLocation.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/Support/Dom.hpp>

namespace clang {
namespace mrdox {

DomLocation::
DomLocation(
    Location const& loc,
    Corpus const& corpus) noexcept
    : loc_(&loc)
    , corpus_(corpus)
{
    (void)corpus_;
}

dom::Value
DomLocation::
get(std::string_view key) const
{
    if(key == "file")
        return loc_->Filename;
    if(key == "line")
        return loc_->LineNumber;
    return nullptr;
}

auto
DomLocation::
props() const ->
    std::vector<std::string_view>
{
    return {
        "file",
        "line"
    };
}

} // mrdox
} // clang
