//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "Support/Operator.hpp"
#include "Support/SafeNames.hpp"
#include "Support/Validate.hpp"
#include <mrdox/Corpus.hpp>
#include <mrdox/Metadata.hpp>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/StringMap.h>
#include <algorithm>

namespace clang {
namespace mrdox {

/*
    Unsafe names:

    destructors
    overloaded operators
    function templates
    class templates
*/
class SafeNames::
    Builder : public Corpus::Visitor
{
    llvm::raw_ostream* os_ = nullptr;
    std::string prefix_;
    std::string temp_;

public:
    Builder(
        Corpus const& corpus)
        : corpus_(corpus)
    {
        prefix_.reserve(512);
        corpus_.visit(globalNamespaceID, *this);
        map.try_emplace(llvm::toStringRef(EmptySID), std::string());

    #ifndef NDEBUG
        //for(auto const& N : map)
            //Assert(validAdocSectionID(N.second));
    #endif
    }

    Builder(
        llvm::raw_ostream& os,
        Corpus const& corpus)
        :
        //os_(&os),
        corpus_(corpus)
    {
        prefix_.reserve(512);
        corpus_.visit(globalNamespaceID, *this);
        map.try_emplace(llvm::toStringRef(EmptySID), std::string());
        if(os_)
            *os_ << "\n\n";
    }
 
    llvm::StringMap<std::string> map;

private:
    using ScopeInfos = std::vector<Info const*>;

    ScopeInfos
    buildScope(
        Scope const& scope)
    {
        ScopeInfos infos;
        //auto overloads = makeOverloadsSet(corpus_, scope);
        infos.reserve(
            scope.Namespaces.size() +
            scope.Records.size() +
            scope.Functions.size() +
            scope.Typedefs.size() +
            scope.Enums.size() +
            scope.Variables.size());
        for(auto const& ref : scope.Namespaces)
            infos.emplace_back(corpus_.find(ref.id));
        for(auto const& ref : scope.Records)
            infos.emplace_back(corpus_.find(ref.id));
        for(auto const& ref : scope.Functions)
            infos.emplace_back(corpus_.find(ref.id));
        for(auto const& ref : scope.Typedefs)
            infos.emplace_back(corpus_.find(ref.id));
        for(auto const& ref : scope.Enums)
            infos.emplace_back(corpus_.find(ref.id));
        for(auto const& ref : scope.Variables)
            infos.emplace_back(corpus_.find(ref.id));
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

    llvm::StringRef
    getSafe(Info const& I)
    {
        if(I.IT != InfoType::IT_function)
            return I.Name;
        auto const& FI = static_cast<
            FunctionInfo const&>(I);
        auto OOK = FI.specs0.get<
            FnFlags0::overloadedOperator,
            OverloadedOperatorKind>();
        if(OOK == OverloadedOperatorKind::OO_None)
            return I.Name;
        temp_ = '0';
        temp_.append(getSafeOperatorName(OOK));
        return temp_;
    }

    void insertScope(ScopeInfos const& infos)
    {
        if(os_)
        {
            std::string temp;
            if( infos.size() > 0 &&
                infos.front()->Namespace.size() > 0)
            {
                auto const& P = corpus_.get<Info>(infos.front()->Namespace[0].id);
                P.getFullyQualifiedName(temp);
                temp.push_back(' ');
            }
            *os_ <<
                "------------------------\n" <<
                "Scope " << temp <<
                "with " << infos.size() << " names:\n\n";
            for(auto const& I : infos)
                *os_ << I->Name << '\n';
            *os_ << '\n';
        }

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
                s.append(getSafe(**it0));
                if(os_)
                    *os_ << getSafe(**it0) << "\n";
                auto result = map.try_emplace(
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
                std::string suffix;
                suffix = std::to_string(i + 1);
                //s.push_back('@');
                suffix.append(getSafe(**it0));
                if(os_)
                    *os_ << suffix << "\n";
                s.append(suffix);
                auto result = map.try_emplace(
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
            prefix_.append(getSafe(J));
            prefix_.push_back('-');
            corpus_.visit(J, *this);
            prefix_.resize(n0);
        }
    }

    void visitInfos(ScopeInfos const& infos)
    {
        auto const n0 = prefix_.size();
        for(auto const I : infos)
        {
            prefix_.append(getSafe(*I));
            prefix_.push_back('-');
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

SafeNames::
SafeNames(
    llvm::raw_ostream& os,
    Corpus const& corpus)
{
    Builder b(os, corpus);
    map_ = std::move(b.map);
}

llvm::StringRef
SafeNames::
get(
    SymbolID const &id) const
{
    auto const it = map_.find(llvm::toStringRef(id));
    Assert(it != map_.end());
    return it->getValue();
}

llvm::StringRef
SafeNames::
get(
    SymbolID const& id,
    char sep,
    std::string& dest) const
{
    auto it = map_.find(llvm::toStringRef(id));
    Assert(it != map_.end());
    if(sep == '.')
        return it->getValue();
    dest = it->getValue();
    std::replace(dest.begin(), dest.end(), '.', sep);
    return dest;
}

llvm::StringRef
SafeNames::
getOverload(
    Info const& P,
    llvm::StringRef name,
    char sep,
    std::string& dest) const
{
    dest = get(P.id);
    dest.push_back(sep);
    dest.push_back('0');
    dest.append(name);
    return dest;
}

} // mrdox
} // clang
