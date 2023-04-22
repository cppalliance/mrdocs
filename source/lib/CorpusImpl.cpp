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

#include "CorpusImpl.hpp"
#include <mrdox/Metadata.hpp>

namespace clang {
namespace mrdox {

//------------------------------------------------

Info*
CorpusImpl::
find(
    SymbolID const& id) noexcept
{
    auto it = InfoMap.find(llvm::toStringRef(id));
    if(it != InfoMap.end())
        return it->second.get();
    return nullptr;
}

Info const*
CorpusImpl::
find(
    SymbolID const& id) const noexcept
{
    auto it = InfoMap.find(llvm::toStringRef(id));
    if(it != InfoMap.end())
        return it->second.get();
    return nullptr;
}

//------------------------------------------------

void
CorpusImpl::
insert(std::unique_ptr<Info> I)
{
    // add to allSymbols
    {
        std::lock_guard<llvm::sys::Mutex> Guard(allSymbolsMutex);
        assert(! isCanonical_);
        allSymbols_.emplace_back(I->USR);
    }

    // This has to come last because we move I.
    {
        std::lock_guard<llvm::sys::Mutex> Guard(infoMutex);
        InfoMap[llvm::toStringRef(I->USR)] = std::move(I);
    }
}

//------------------------------------------------

struct CorpusImpl::Temps
{
    std::string s0;
    std::string s1;
};

bool
CorpusImpl::
canonicalize(Reporter& R)
{
    if(isCanonical_)
        return true;
    assert(exists(globalNamespaceID));
    auto& I = get<NamespaceInfo>(globalNamespaceID);

    if(config_->verbose())
        R.print("Canonicalizing...");

    Temps t;

    if(! canonicalize(I, t, R))
        return false;

    if(! canonicalize(allSymbols_, t, R))
        return false;

    isCanonical_ = true;
    return true;
}

bool
CorpusImpl::
canonicalize(
    std::vector<SymbolID>& list, Temps& t, Reporter& R)
{
    // Sort by fully qualified name
    llvm::sort(
        list,
        [&](SymbolID const& id0,
            SymbolID const& id1) noexcept
        {
            auto s0 = get<Info>(id0).getFullyQualifiedName(t.s0);
            auto s1 = get<Info>(id1).getFullyQualifiedName(t.s1);
            return compareSymbolNames(s0, s1);
        });
    return true;
}


bool
CorpusImpl::
canonicalize(
    NamespaceInfo& I,
    Temps& t,
    Reporter& R)
{
    I.javadoc.calculateBrief();
    if(! canonicalize(I.Children, t, R))
        return false;
    return true;
}

bool
CorpusImpl::
canonicalize(
    RecordInfo& I,
    Temps& t,
    Reporter& R)
{
    I.javadoc.calculateBrief();
    canonicalize(I.Children, t, R);
    canonicalize(I.Members, t, R);
    return true;
}

bool
CorpusImpl::
canonicalize(
    FunctionInfo& I,
    Temps& t,
    Reporter& R)
{
    I.javadoc.calculateBrief();
    return true;
}

bool
CorpusImpl::
canonicalize(
    EnumInfo& I,
    Temps& t,
    Reporter& R)
{
    I.javadoc.calculateBrief();
    return true;
}

bool
CorpusImpl::
canonicalize(
    TypedefInfo& I,
    Temps& t,
    Reporter& R)
{
    I.javadoc.calculateBrief();
    return true;
}

bool
CorpusImpl::
canonicalize(
    Scope& I,
    Temps& t,
    Reporter& R)
{
    std::sort(
        I.Namespaces.begin(),
        I.Namespaces.end(),
        [this, &t](Reference& ref0, Reference& ref1)
        {
            return compareSymbolNames(
                get<Info>(ref0.USR).getFullyQualifiedName(t.s0),
                get<Info>(ref1.USR).getFullyQualifiedName(t.s1));
        });
    std::sort(
        I.Records.begin(),
        I.Records.end(),
        [this, &t](Reference& ref0, Reference& ref1)
        {
            return compareSymbolNames(
                get<Info>(ref0.USR).getFullyQualifiedName(t.s0),
                get<Info>(ref1.USR).getFullyQualifiedName(t.s1));
        });
    std::sort(
        I.Functions.begin(),
        I.Functions.end(),
        [this, &t](Reference& ref0, Reference& ref1)
        {
            return compareSymbolNames(
                get<Info>(ref0.USR).getFullyQualifiedName(t.s0),
                get<Info>(ref1.USR).getFullyQualifiedName(t.s1));
        });
    // These seem to be non-copyable
#if 0
    std::sort(
        I.Enums.begin(),
        I.Enums.end(),
        [this, &t](EnumInfo& I0, EnumInfo& I1)
        {
            return compareSymbolNames(
                I0.getFullyQualifiedName(t.s0),
                I1.getFullyQualifiedName(t.s1));
        });
    std::sort(
        I.Typedefs.begin(),
        I.Typedefs.end(),
        [this, &t](TypedefInfo& I0, TypedefInfo& I1)
        {
            return compareSymbolNames(
                I0.getFullyQualifiedName(t.s0),
                I1.getFullyQualifiedName(t.s1));
        });
#endif
    for(auto& ref : I.Namespaces)
        if(! canonicalize(get<NamespaceInfo>(ref.USR), t, R))
            return false;
    for(auto& ref : I.Records)
        if(! canonicalize(get<RecordInfo>(ref.USR), t, R))
            return false;
    for(auto& ref : I.Functions)
        if(! canonicalize(get<FunctionInfo>(ref.USR), t, R))
            return false;
    for(auto& J: I.Enums)
        if(! canonicalize(J, t, R))
            return false;
    for(auto& J: I.Typedefs)
        if(! canonicalize(J, t, R))
            return false;
    return true;
}

bool
CorpusImpl::
canonicalize(
    std::vector<Reference>& list,
    Temps& t,
    Reporter& R)
{
    std::sort(
        list.begin(),
        list.end(),
        [this, &t](Reference& ref0, Reference& ref1)
        {
            return compareSymbolNames(
                get<Info>(ref0.USR).getFullyQualifiedName(t.s0),
                get<Info>(ref1.USR).getFullyQualifiedName(t.s1));
        });
    return true;
}

bool
CorpusImpl::
canonicalize(
    llvm::SmallVectorImpl<MemberTypeInfo>& list,
    Temps& t,
    Reporter& R)
{
    for(auto J : list)
        J.javadoc.calculateBrief();
    return true;
}

} // mrdox
} // clang
