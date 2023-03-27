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

//------------------------------------------------

void
FunctionOverloads::
insert(
    clang::doc::FunctionInfo I)
{
    assert(I.Name == name);
    v_.emplace_back(std::move(I));
}

void
FunctionOverloads::
merge(
    FunctionOverloads&& other)
{
    clang::doc::reduceChildren(
        v_, std::move(other.v_));
}

FunctionOverloads::
FunctionOverloads(
    clang::doc::FunctionInfo I)
    : name(I.Name)
{
    v_.emplace_back(std::move(I));
}

//------------------------------------------------

void
FunctionList::
insert(
    clang::doc::FunctionInfo I)
{
    //assert(I.Access == access);
    auto it = find(I.Name);
    if(it != end())
    {
        // new function
        it->insert(std::move(I));
        return;
    }

    // overloaded function
    v_.emplace_back(std::move(I));
}

void
FunctionList::
merge(
    FunctionList&& other)
{
    for(auto& v : *this)
    {
        auto it = other.find(v.name);
        if(it == other.end())
            continue;
        v.merge(std::move(*it));
        other.v_.erase(it);
    }
    for(auto& v : other)
        v_.emplace_back(std::move(v));
    other.v_.clear();
}

auto
FunctionList::
find(
    llvm::StringRef name) noexcept ->
        iterator
{
    auto it = begin();
    for(;it != end(); ++it)
        if(it->name == name)
            return it;
    return it;
}

} // mrdox
