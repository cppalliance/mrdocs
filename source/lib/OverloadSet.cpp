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

#include <mrdox/Corpus.hpp>
#include <mrdox/Function.hpp>
#include <mrdox/OverloadSet.hpp>
#include <mrdox/Scope.hpp>

namespace clang {
namespace mrdox {

std::vector<OverloadSet>
makeOverloadSet(
    Corpus const& corpus,
    Scope const& scope,
    std::function<bool(FunctionInfo const&)> filter)
{
    std::vector<OverloadSet> result;
    std::vector<FunctionInfo const*> list;
    list.reserve(scope.Functions.size());
    for(auto const& ref : scope.Functions)
    {
        auto const& I = corpus.get<FunctionInfo>(ref.USR);
        if(filter(I))
            list.push_back(&I);
    }
    if(list.empty())
        return {};
    std::sort(list.begin(), list.end(),
        []( FunctionInfo const* f0,
            FunctionInfo const* f1)
        {
            return f0->Name < f1->Name;
        });
    auto it0 = list.begin();
    auto it = it0;
    do
    {
        ++it;
        if( it == list.end() ||
            (*it)->Name != (*it0)->Name)
        {
            result.emplace_back(
                OverloadSet{
                    (*it0)->Name,
                    std::vector<FunctionInfo const*>(it0, it)});
            it0 = it;
        }
    }
    while(it != list.end());
    return result;
}

} // mrdox
} // clang
