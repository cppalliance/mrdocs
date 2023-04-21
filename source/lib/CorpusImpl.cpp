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
insert(std::unique_ptr<Info> Ip)
{
    assert(! isCanonical_);

    auto& I = *Ip;

    // VFALCO Better to do this in canonicalize
    // Clean up the javadoc
    /*
    I.javadoc.calculateBrief();
    if(I.IT == InfoType::IT_record)
    {
        auto& J = static_cast<RecordInfo&>(I);
        for(auto& K : J.Members)
            K.javadoc.calculateBrief();
    }
    */

    // Add a reference to this Info in the Index
    insertIntoIndex(I);

    // This has to come last because we move Ip.
    // Store the Info in the result map
    {
        std::lock_guard<llvm::sys::Mutex> Guard(infoMutex);
        InfoMap[llvm::toStringRef(I.USR)] = std::move(Ip);
    }
    // CANNOT touch I or Ip here!
}

// A function to add a reference to Info in Idx.
// Given an Info X with the following namespaces: [B,A]; a reference to X will
// be added in the children of a reference to B, which should be also a child of
// a reference to A, where A is a child of Idx.
//   Idx
//    |-- A
//        |--B
//           |--X
// If the references to the namespaces do not exist, they will be created. If
// the references already exist, the same one will be used.
void
CorpusImpl::
insertIntoIndex(
    Info const& I)
{
    assert(! isCanonical_);

    std::lock_guard<llvm::sys::Mutex> Guard(allSymbolsMutex);

    // Index pointer that will be moving through Idx until the first parent
    // namespace of Info (where the reference has to be inserted) is found.
    Index* pi = &Idx;
    // The Namespace vector includes the upper-most namespace at the end so the
    // loop will start from the end to find each of the namespaces.
    for (const auto& R : llvm::reverse(I.Namespace))
    {
        // Look for the current namespace in the children of the index pi is
        // pointing.
        auto It = llvm::find(pi->Children, R.USR);
        if (It != pi->Children.end())
        {
            // If it is found, just change pi to point the namespace reference found.
            pi = &*It;
        }
        else
        {
            // If it is not found a new reference is created
            pi->Children.emplace_back(R.USR, R.Name, R.RefType, R.Path);
            // pi is updated with the reference of the new namespace reference
            pi = &pi->Children.back();
        }
    }
    // Look for Info in the vector where it is supposed to be; it could already
    // exist if it is a parent namespace of an Info already passed to this
    // function.
    auto It = llvm::find(pi->Children, I.USR);
    if (It == pi->Children.end())
    {
        // If it is not in the vector it is inserted
        pi->Children.emplace_back(I.USR, I.extractName(), I.IT,
            I.Path);
    }
    else
    {
        // If it not in the vector we only check if Path and Name are not empty
        // because if the Info was included by a namespace it may not have those
        // values.
        if (It->Path.empty())
            It->Path = I.Path;
        if (It->Name.empty())
            It->Name = I.extractName();
    }

    // also insert into allSymbols
    allSymbols_.emplace_back(I.USR);
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
