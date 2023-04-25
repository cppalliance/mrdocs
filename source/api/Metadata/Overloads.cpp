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
#include <mrdox/Metadata/Function.hpp>
#include <mrdox/Metadata/Overloads.hpp>
#include <mrdox/Metadata/Scope.hpp>

namespace clang {
namespace mrdox {

template<class Pred>
static
OverloadsSet
makeOverloadsSetImpl(
    Corpus const& corpus,
    Scope const& scope,
    Pred&& pred)
{
    OverloadsSet result;
    std::vector<FunctionInfo const*> temp;
    temp.reserve(scope.Functions.size());
    for(auto const& ref : scope.Functions)
    {
        auto const& I = corpus.get<FunctionInfo>(ref.id);
        if(pred(I))
            temp.push_back(&I);
    }
    if(temp.empty())
        return result;

    std::sort(temp.begin(), temp.end(),
        []( FunctionInfo const* f0,
            FunctionInfo const* f1)
        {
            return f0->Name < f1->Name;
        });
    auto it0 = temp.begin();
    auto it = it0;
    do
    {
        ++it;
        if( it == temp.end() ||
            (*it)->Name != (*it0)->Name)
        {
            result.list.emplace_back(
                Overloads{
                    (*it0)->Name,
                    std::vector<FunctionInfo const*>(it0, it)});
            it0 = it;
        }
    }
    while(it != temp.end());
    return result;
}

OverloadsSet
makeOverloadsSet(
    Corpus const& corpus,
    Scope const& scope)
{
    return makeOverloadsSetImpl(corpus, scope,
        [](FunctionInfo const& I)
        {
            return true;
        });
}

OverloadsSet
makeOverloadsSet(
    Corpus const& corpus,
    Scope const& scope,
    AccessSpecifier access)
{
    return makeOverloadsSetImpl(corpus, scope,
        [access](FunctionInfo const& I)
        {
            return I.Access == access;
        });
}

} // mrdox
} // clang
