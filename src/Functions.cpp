//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "Functions.h"
#include "Reduce.h"
#include "Representation.h"

// we assume that there are 4 access controls
static_assert(
    clang::AccessSpecifier::AS_none >
    clang::AccessSpecifier::AS_private);
static_assert(
    clang::AccessSpecifier::AS_none >
    clang::AccessSpecifier::AS_protected);
static_assert(
    clang::AccessSpecifier::AS_none >
    clang::AccessSpecifier::AS_public);

namespace mrdox {

FunctionOverloads::
FunctionOverloads(
    clang::doc::FunctionInfo I)
    : name(I.Name)
{
    list.emplace_back(std::move(I));
}

void
FunctionOverloads::
merge(
    FunctionOverloads&& other)
{
    //if(this->name.empty())
        //this->name = std::move(other.name);
    //if(other.name != this->name)
        //return;
    clang::doc::reduceChildren(list, std::move(other.list));
}

//------------------------------------------------

void
ScopedFunctions::
insert(
    clang::doc::FunctionInfo I)
{
    auto& v = overloads[I.Access];
    auto it = find(v, I.Name);
    if(it != v.end())
    {
        // new function
        v.emplace_back(std::move(I));
        return;
    }

    // overloaded function
    v.emplace_back(std::move(I));
}

void
ScopedFunctions::
merge(
    ScopedFunctions&& other)
{
    for(std::size_t i = 0; i < 4; ++i)
    {
        auto& v0 = overloads[i];
        auto& v1 = other.overloads[i];
        for(auto& fo : v0)
        {
            auto it = other.find(v1, fo.name);
            if(it == v1.end())
                continue;
            fo.merge(std::move(*it));
            v1.erase(it);
        }
        for(auto& fo : v1)
            v0.emplace_back(std::move(fo));
    }
}

ScopedFunctions::
ScopedFunctions()
{
    overloads.resize(4);
}

auto
ScopedFunctions::
find(
    Functions& v,
    llvm::StringRef name) noexcept ->
        Functions::iterator
{
    auto it = v.begin();
    for(;it != v.end(); ++it)
        if(it->name == name)
            return it;
    return v.end();
}

auto
ScopedFunctions::
find(
    clang::doc::FunctionInfo const& I) noexcept ->
        Functions::iterator
{
    return find(overloads[I.Access], I.Name);
}

} // mrdox
