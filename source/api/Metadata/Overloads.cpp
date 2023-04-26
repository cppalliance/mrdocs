//
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
#include <llvm/ADT/STLExtras.h>

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

//------------------------------------------------

NamespaceOverloads::
NamespaceOverloads(
    std::vector<FunctionInfo const*> data)
    : data_(std::move(data))
{
    // Sort to group the overloads, preserving order
    llvm::stable_sort(data_,
        []( FunctionInfo const* f0,
            FunctionInfo const* f1)
        {
            return compareSymbolNames(f0->Name, f1->Name) < 0;
        });

    // Find the end of the range of each overload set
    auto it0 = data_.begin();
    while(it0 != data_.end())
    {
        auto it = std::find_if_not(
            it0 + 1, data_.end(),
            [it0](auto I)
            {
                return (*it0)->Name.compare_insensitive(I->Name) == 0;
            });
        list.push_back({
            { (*it0)->Name.data(), (*it0)->Name.size() },
            { it0, it } });
        it0 = it;
    }
}

NamespaceOverloads
makeNamespaceOverloads(
    std::vector<Reference> const& list,
    Corpus const& corpus)
{
    std::vector<FunctionInfo const*> data;
    data.reserve(list.size());
    for(auto const& ref : list)
    {
        auto const& I = corpus.get<FunctionInfo>(ref.id);
        data.push_back(&I);
    }

    return NamespaceOverloads(std::move(data));
}

} // mrdox
} // clang
