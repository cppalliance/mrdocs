//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "Support/Radix.hpp"
#include <mrdox/Dom/DomType.hpp>
#include <mrdox/Dom/DomSymbol.hpp>

namespace clang {
namespace mrdox {

DomType::
DomType(
    TypeInfo const& I,
    Corpus const& corpus) noexcept
    : I_(&I)
    , corpus_(corpus)
{
    if(I_->id != SymbolID::zero)
        J_ = corpus_.find(I_->id);
    else
        J_ = nullptr;
}

dom::Value
DomType::
get(std::string_view key) const
{
    if(key == "id")
        return toBase16(I_->id);
    if(key == "name")
    {
        if(J_)
            return dom::nonEmptyString(J_->Name);
        return dom::nonEmptyString(I_->Name);
    }
    if(key == "symbol")
    {
        if(! J_)
            return nullptr;
        return visit(
            *J_,
            [&]<class T>(T const& I) ->
                dom::ObjectPtr
            {
                return dom::makePointer<
                    DomSymbol<T>>(I, corpus_);
            });
    }
    return nullptr;
}

auto
DomType::
props() const ->
    std::vector<std::string_view>
{
    return { "id", "name", "symbol" };
}

} // mrdox
} // clang
