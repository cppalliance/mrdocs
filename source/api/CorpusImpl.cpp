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
    std::lock_guard<llvm::sys::Mutex> Guard(mutex_);

    Assert(! isCanonical_);
    index_.emplace_back(I.get());

    // This has to come last because we move I.
    InfoMap[llvm::toStringRef(I->id)] = std::move(I);
}

//------------------------------------------------
//
// MutableVisitor
//
//------------------------------------------------

void
CorpusImpl::
visit(SymbolID id, MutableVisitor& f)
{
    visit(get<Info>(id), f);
}

void
CorpusImpl::
visit(Scope& I, MutableVisitor& f)
{
    for(auto& ref : I.Namespaces)
        visit(get<NamespaceInfo>(ref.id), f);
    for(auto& ref : I.Records)
        visit(get<RecordInfo>(ref.id), f);
    for(auto& ref : I.Functions)
        visit(get<FunctionInfo>(ref.id), f);
    for(auto& ref : I.Typedefs)
        visit(get<TypedefInfo>(ref.id), f);
    for(auto& ref : I.Enums)
        visit(get<EnumInfo>(ref.id), f);
    for(auto& ref : I.Variables)
        visit(get<VariableInfo>(ref.id), f);
}

void
CorpusImpl::
visit(Info& I, MutableVisitor& f)
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
        return f.visit(static_cast<VariableInfo&>(I));
    default:
        llvm_unreachable("wrong InfoType for viist");
    }
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
    Reporter& R_;

public:
    Canonicalizer(
        CorpusImpl& corpus,
        Reporter& R) noexcept
        : corpus_(corpus)
        , R_(R)
    {
    }

    void visit(NamespaceInfo& I) override
    {
        postProcess(I);
        canonicalize(I.Children);
        corpus_.visit(I.Children, *this);
    }

    void visit(RecordInfo& I) override
    {
        postProcess(I);
        canonicalize(I.Children);
        canonicalize(I.Members);
        canonicalize(I.Friends);
        corpus_.visit(I.Children, *this);
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

    void visit(VariableInfo& I) override
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

    void canonicalize(Scope& scope) noexcept
    {
        canonicalize(scope.Namespaces);
        canonicalize(scope.Records);
        canonicalize(scope.Functions);
        canonicalize(scope.Typedefs);
        canonicalize(scope.Enums);
        canonicalize(scope.Variables);
    }

    void canonicalize(std::vector<Reference>& list) noexcept
    {
        // Sort by symbol ID
        llvm::sort(list,
            [&](Reference const& ref0,
                Reference const& ref1) noexcept
            {
                return std::memcmp(
                    ref0.id.data(),
                    ref1.id.data(),
                    ref0.id.size()) < 0;
            });
    }

    void canonicalize(llvm::SmallVectorImpl<SymbolID>& list) noexcept
    {
        // Sort by symbol ID
        llvm::sort(list,
            [&](SymbolID const& id0,
                SymbolID const& id1) noexcept
            {
                return std::memcmp(
                    id0.data(),
                    id1.data(),
                    id0.size()) < 0;
            });
    }

    void canonicalize(llvm::SmallVectorImpl<MemberTypeInfo>& list)
    {
    }
};

void
CorpusImpl::
canonicalize(
    Reporter& R)
{
    if(isCanonical_)
        return;
    if(config_->verbose())
        R.print("Canonicalizing...");
    Canonicalizer cn(*this, R);
    visit(globalNamespaceID, cn);
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
