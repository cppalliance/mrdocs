//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Dom/DomArray.hpp>
#include <mrdox/Dom/DomLocation.hpp>
#include <mrdox/Dom/DomSource.hpp>

namespace clang {
namespace mrdox {

DomSource::
DomSource(
    SourceInfo const& I,
    Corpus const& corpus) noexcept
    : I_(I)
    , corpus_(corpus)
{
}

dom::Value
DomSource::
get(std::string_view key) const
{
    if(key == "def")
    {
        if(! I_.DefLoc)
            return nullptr;
        return dom::makePointer<DomLocation>(
            *I_.DefLoc, corpus_);
    }
    if(key == "decl")
        return dom::makePointer<DomArray<
            Location, DomLocation>>(I_.Loc, corpus_);
    return nullptr;
}

auto
DomSource::
props() const ->
    std::vector<std::string_view>
{
    return { "def", "decl" };
}

} // mrdox
} // clang
