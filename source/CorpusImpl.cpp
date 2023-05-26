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
#include <llvm/ADT/STLExtras.h>

namespace clang {
namespace mrdox {

//------------------------------------------------

Info*
CorpusImpl::
find(
    SymbolID const& id) noexcept
{
    auto it = InfoMap.find(id);
    if(it != InfoMap.end())
        return it->second.get();
    return nullptr;
}

Info const*
CorpusImpl::
find(
    SymbolID const& id) const noexcept
{
    auto it = InfoMap.find(id);
    if(it != InfoMap.end())
        return it->second.get();
    return nullptr;
}

//------------------------------------------------

void
CorpusImpl::
insert(std::unique_ptr<Info> I)
{
    std::lock_guard<llvm::sys::Mutex> Guard(mutex_);

    Assert(! isCanonical_);
    index_.emplace_back(I.get());

    // This has to come last because we move I.
    InfoMap[I->id] = std::move(I);
}

//------------------------------------------------
//
// MutableVisitor
//
//------------------------------------------------

void
CorpusImpl::
traverse(
    MutableVisitor& f,
    Info& I)
{
    switch(I.IT)
    {
    case InfoType::IT_namespace:
        return f.visit(static_cast<NamespaceInfo&>(I));
    case InfoType::IT_record:
        return f.visit(static_cast<RecordInfo&>(I));
    case InfoType::IT_function:
        return f.visit(static_cast<FunctionInfo&>(I));
    case InfoType::IT_typedef:
        return f.visit(static_cast<TypedefInfo&>(I));
    case InfoType::IT_enum:
        return f.visit(static_cast<EnumInfo&>(I));
    case InfoType::IT_variable:
        return f.visit(static_cast<VarInfo&>(I));
    case InfoType::IT_field:
        return f.visit(static_cast<FieldInfo&>(I));
    default:
        llvm_unreachable("wrong InfoType for visit");
    }
}

void
CorpusImpl::
traverse(
    MutableVisitor& f,
    NamespaceInfo& I)
{
    for(auto const& ref : I.Children.Namespaces)
        f.visit(get<NamespaceInfo>(ref.id));
    for(auto const& ref : I.Children.Records)
        f.visit(get<RecordInfo>(ref.id));
    for(auto const& ref : I.Children.Functions)
        f.visit(get<FunctionInfo>(ref.id));
    for(auto const& ref : I.Children.Typedefs)
        f.visit(get<TypedefInfo>(ref.id));
    for(auto const& ref : I.Children.Enums)
        f.visit(get<EnumInfo>(ref.id));
    for(auto const& ref : I.Children.Vars)
        f.visit(get<VarInfo>(ref.id));
}

void
CorpusImpl::
traverse(
    MutableVisitor& f,
    RecordInfo& I)
{
    for(auto const& t : I.Members.Records)
        f.visit(get<RecordInfo>(t.id));
    for(auto const& t : I.Members.Functions)
        f.visit(get<FunctionInfo>(t.id));
    for(auto const& t : I.Members.Types)
        f.visit(get<TypedefInfo>(t.id));
    for(auto const& t : I.Members.Enums)
        f.visit(get<EnumInfo>(t.id));
    for(auto const& t : I.Members.Fields)
        f.visit(get<FieldInfo>(t.id));
    for(auto const& t : I.Members.Vars)
        f.visit(get<VarInfo>(t.id));
}

void
CorpusImpl::
traverse(MutableVisitor& f, SymbolID id)
{
    traverse(f, get<Info>(id));
}

//------------------------------------------------
//
// Canonicalizer
//
//------------------------------------------------

class CorpusImpl::
    Canonicalizer : public MutableVisitor
{
    CorpusImpl& corpus_;

public:
    Canonicalizer(
        CorpusImpl& corpus) noexcept
        : corpus_(corpus)
    {
    }

    void visit(NamespaceInfo& I) override
    {
        postProcess(I);
        canonicalize(I.Children.Namespaces);
        canonicalize(I.Children.Records);
        canonicalize(I.Children.Functions);
        canonicalize(I.Children.Typedefs);
        canonicalize(I.Children.Enums);
        canonicalize(I.Children.Vars);
        corpus_.traverse(*this, I);
    }

    void visit(RecordInfo& I) override
    {
        postProcess(I);
        // VFALCO Is this needed?
        canonicalize(I.Friends);
        corpus_.traverse(*this, I);
    }

    void visit(FunctionInfo& I) override
    {
        postProcess(I);
    }

    void visit(TypedefInfo& I) override
    {
        postProcess(I);
    }

    void visit(EnumInfo& I) override
    {
        postProcess(I);
    }

    void visit(VarInfo& I) override
    {
        postProcess(I);
    }

    void visit(FieldInfo& I) override
    {
        postProcess(I);
    }

    //--------------------------------------------

    void postProcess(Info& I)
    {
        if(I.javadoc)
            I.javadoc->postProcess();
    }

    //--------------------------------------------

    void canonicalize(std::vector<Reference>& list) noexcept
    {
        // Sort by symbol ID
        llvm::sort(list,
            [&](Reference const& ref0,
                Reference const& ref1) noexcept
            {
                return ref0.id < ref1.id;
            });
    }

    void canonicalize(llvm::SmallVectorImpl<SymbolID>& list) noexcept
    {
        // Sort by symbol ID
        llvm::sort(list,
            [&](SymbolID const& id0,
                SymbolID const& id1) noexcept
            {
                return id0 < id1;
            });
    }
};

void
CorpusImpl::
canonicalize(
    Reporter& R)
{
    if(isCanonical_)
        return;
    if(config_->verboseOutput)
        R.print("Canonicalizing...");
    Canonicalizer cn(*this);
    traverse(cn, SymbolID::zero);
    std::string temp0;
    std::string temp1;
    llvm::sort(index_,
        [&](Info const* I0,
            Info const* I1)
        {
            return compareSymbolNames(
                I0->getFullyQualifiedName(temp0),
                I1->getFullyQualifiedName(temp1)) < 0;
        });
    isCanonical_ = true;
}

} // mrdox
} // clang
