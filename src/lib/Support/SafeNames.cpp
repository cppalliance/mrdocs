//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "lib/Support/Radix.hpp"
#include "lib/Support/SafeNames.hpp"
#include "lib/Support/Validate.hpp"
#include <mrdox/Corpus.hpp>
#include <mrdox/Metadata.hpp>
#include <mrdox/Platform.hpp>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/ADT/StringMap.h>
#include <algorithm>

namespace clang {
namespace mrdox {

namespace {

/*
    Unsafe names:

    destructors
    overloaded operators
    function templates
    class templates
*/
class PrettyBuilder
{
    llvm::raw_ostream* os_ = nullptr;
    std::string prefix_;
    std::string temp_;

public:
    explicit
    PrettyBuilder(
        Corpus const& corpus)
        : corpus_(corpus)
    {
        prefix_.reserve(512);
        visit(corpus_.globalNamespace(), *this);
        /* auto result =*/ map.try_emplace(
            llvm::StringRef(SymbolID::zero), std::string());

    #ifndef NDEBUG
        //for(auto const& N : map)
            //MRDOX_ASSERT(validAdocSectionID(N.second));
    #endif
    }

    PrettyBuilder(
        llvm::raw_ostream& os,
        Corpus const& corpus)
        :
        //os_(&os),
        corpus_(corpus)
    {
        prefix_.reserve(512);
        visit(corpus_.globalNamespace(), *this);
        /* auto result =*/ map.try_emplace(
            llvm::StringRef(SymbolID::zero), std::string());
        if(os_)
            *os_ << "\n\n";
    }

    llvm::StringMap<std::string> map;

    using ScopeInfos = std::vector<Info const*>;

    ScopeInfos
    buildScope(
        NamespaceInfo const& I)
    {
        ScopeInfos infos;
        infos.reserve(I.Members.size());
        for(auto const& id : I.Members)
            infos.emplace_back(corpus_.find(id));
        // KRYSTIAN FIXME: include specializations
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

    ScopeInfos
    buildScope(
        RecordInfo const& I)
    {
        ScopeInfos infos;
        infos.reserve(I.Members.size());
        for(auto const& id : I.Members)
            infos.emplace_back(corpus_.find(id));
        // KRYSTIAN FIXME: include specializations and friends
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
        if(I.Kind != InfoKind::Function)
            return I.Name;
        auto const& FI = static_cast<
            FunctionInfo const&>(I);
        auto OOK = FI.specs0.overloadedOperator.get();
        if(OOK == OperatorKind::None)
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
                auto const& P = corpus_.get<Info>(infos.front()->Namespace[0]);
                corpus_.getFullyQualifiedName(P, temp);
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
                    return
                        llvm::StringRef((*it0)->Name).compare_insensitive(
                        llvm::StringRef(I->Name)) != 0;
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
                /*auto result =*/ map.try_emplace(
                    llvm::StringRef((*it0)->id),
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
                /*auto result =*/ map.try_emplace(
                    llvm::StringRef(it0[i]->id),
                    std::move(s));
            }
            it0 = it;
        }
    }

    void visitInfos(ScopeInfos const& infos)
    {
        auto const n0 = prefix_.size();
        for(auto const I : infos)
        {
            prefix_.append(getSafe(*I));
            prefix_.push_back('-');
            ::clang::mrdox::visit(*I, *this);
            prefix_.resize(n0);
        }
    }

    //--------------------------------------------

    template<class T>
    void operator()(T const& I)
    {
        if constexpr(
            T::isNamespace() ||
            T::isRecord())
        {
            auto infos = buildScope(I);
            insertScope(infos);
            visitInfos(infos);
        }
        else
        {
        }
    }

private:
    Corpus const& corpus_;
};

//------------------------------------------------

// Always works but isn't the prettiest...
class UglyBuilder
{
    Corpus const& corpus_;

public:
    llvm::StringMap<std::string> map;

    explicit
    UglyBuilder(
        Corpus const& corpus)
        : corpus_(corpus)
    {
        llvm::SmallString<64> temp;
        for(Info const* I : corpus_.index())
            map.try_emplace(
                llvm::StringRef(I->id),
                toBase16(I->id, true));
    }
};

} // (anon)

//------------------------------------------------

SafeNames::
SafeNames(
    llvm::raw_ostream& os,
    Corpus const& corpus)
    : corpus_(corpus)
    //, map_(PrettyBuilder(corpus).map)
    , map_(UglyBuilder(corpus).map)
{
}

SafeNames::
SafeNames(
    Corpus const& corpus)
    : corpus_(corpus)
    //, map_(PrettyBuilder(corpus).map)
    , map_(UglyBuilder(corpus).map)
{
}

llvm::StringRef
SafeNames::
get(
    SymbolID const &id) const noexcept
{
    auto const it = map_.find(llvm::StringRef(id));
    MRDOX_ASSERT(it != map_.end());
    return it->getValue();
}

std::vector<llvm::StringRef>&
SafeNames::
getPath(
    std::vector<llvm::StringRef>& dest,
    SymbolID id) const
{
    auto const& Parents = corpus_.get<Info>(id).Namespace;
    dest.clear();
    dest.reserve(1 + Parents.size());
    dest.push_back(get(id));
    for(auto const& id : llvm::reverse(Parents))
        dest.push_back(get(id));
    return dest;
}

} // mrdox
} // clang
