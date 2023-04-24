//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "SafeNames.hpp"

#include <mrdox/Corpus.hpp>
#include <mrdox/Metadata.hpp>
#include <llvm/ADT/StringMap.h>
#include <algorithm>

namespace clang {
namespace mrdox {
namespace adoc {

class SafeNames::
    Builder : public Corpus::Visitor
{
    std::string prefix_;

public:
    Builder(
        Corpus const& corpus)
        : corpus_(corpus)
    {
        prefix_.reserve(512);
        corpus_.visit(globalNamespaceID, *this);
    }

    llvm::StringMap<std::string> map;

private:
    using ScopeInfos = std::vector<Info const*>;

    std::vector<std::string>
    getNames()
    {
        std::vector<std::string> names;
        for(auto const& ref : map)
            names.push_back(ref.getValue());
        llvm::sort(names,
            [](llvm::StringRef const& n0, llvm::StringRef const& n1)
            {
                return compareSymbolNames(n0, n1) < 0;
            });
        return names;
    }

    ScopeInfos
    buildScope(
        Scope const& scope)
    {
        ScopeInfos infos;
        auto overloads = makeOverloadsSet(corpus_, scope);
        infos.reserve(
            scope.Namespaces.size() +
            scope.Records.size() +
            overloads.list.size() +
            scope.Typedefs.size() +
            scope.Enums.size());
        for(auto const& ref : scope.Namespaces)
            infos.emplace_back(corpus_.find(ref.id));
        for(auto const& ref : scope.Records)
            infos.emplace_back(corpus_.find(ref.id));
        for(auto const& ref : scope.Functions)
            infos.emplace_back(corpus_.find(ref.id));
        for(auto const& I : scope.Typedefs)
            infos.emplace_back(&I);
        for(auto const& I : scope.Enums)
            infos.emplace_back(&I);
        if(infos.size() < 2)
            return infos;
        std::string s0, s1;
        llvm::sort(infos,
            [&](Info const* I0, Info const* I1)
            {
                return compareSymbolNames(I0->Name, I1->Name) < 0;
            });
        return infos;
    }

    void insertScope(ScopeInfos const& infos)
    {
        auto it0 = infos.begin();
        while(it0 != infos.end())
        {
            auto it = std::find_if(
                it0 + 1, infos.end(),
                [it0](auto I)
                {
                    return (*it0)->Name.compare_insensitive(I->Name) != 0;
                });
            std::size_t n = std::distance(it0, it);
            if(n < 2)
            {
                // unique
                std::string s;
                s.assign(prefix_);
                s.append((*it0)->Name.data(), (*it0)->Name.size());
                map.try_emplace(
                    llvm::toStringRef((*it0)->id),
                    std::move(s));
                it0 = it;
                continue;
            }
            // conflicting
            for(std::size_t i = 0; i < n; ++i)
            {
                std::string s;
                s.assign(prefix_);
                s.append((*it0)->Name.data(), (*it0)->Name.size());
                s.push_back('@');
                s.append(std::to_string(i));
                map.try_emplace(
                    llvm::toStringRef(it0[i]->id),
                    std::move(s));
            }
            it0 = it;
        }
    }

    void visitScope(Scope const& scope)
    {
        auto const n0 = prefix_.size();
        for(auto const& ref : scope.Namespaces)
        {
            Info const& J(corpus_.get<Info>(ref.id));
            llvm::StringRef name = J.Name;
            prefix_.append(name.data(), name.size());
            prefix_.push_back('.');
            corpus_.visit(J, *this);
            prefix_.resize(n0);
        }
    }

    void visitInfos(ScopeInfos const& infos)
    {
        auto const n0 = prefix_.size();
        for(auto const I : infos)
        {
            llvm::StringRef name = I->Name;
            prefix_.append(name.data(), name.size());
            prefix_.push_back('.');
            corpus_.visit(*I, *this);
            prefix_.resize(n0);
        }
    }

    //--------------------------------------------

    bool visit(NamespaceInfo const& I) override
    {
        auto infos = buildScope(I.Children);
        insertScope(infos);
        visitInfos(infos);
        return true;
    }

    bool visit(RecordInfo const& I) override
    {
        auto infos = buildScope(I.Children);
        insertScope(infos);
        visitInfos(infos);
        return true;
    }

    bool visit(Overloads const& I) override
    {
        return true;
    }

    bool visit(TypedefInfo const& I) override
    {
        return true;
    }

    bool visit(EnumInfo const& I) override
    {
        return true;
    }

private:
    Corpus const& corpus_;
};

//------------------------------------------------

SafeNames::
SafeNames(
    Corpus const& corpus)
{
    Builder b(corpus);
    map_ = std::move(b.map);
}

llvm::StringRef
SafeNames::
get(SymbolID const &id) const noexcept
{
    return map_.find(llvm::toStringRef(id))->getValue();
}

} // adoc
} // mrdox
} // clang
