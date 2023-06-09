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

#include "AST/Bitcode.hpp"
#include "AST/FrontendAction.hpp"
#include "CorpusImpl.hpp"
#include "Metadata/Reduce.hpp"
#include "Support/Error.hpp"
#include <mrdox/Support/Report.hpp>
#include <mrdox/Metadata.hpp>
#include <mrdox/Support/Report.hpp>
#include <llvm/ADT/STLExtras.h>

namespace clang {
namespace mrdox {

// A standalone function to call to merge a vector of infos into one.
// This assumes that all infos in the vector are of the same type, and will fail
// if they are different.
// Dispatch function.
llvm::Expected<std::unique_ptr<Info>>
mergeInfos(std::vector<std::unique_ptr<Info>>& Values)
{
    if (Values.empty() || !Values[0])
        return llvm::createStringError(llvm::inconvertibleErrorCode(),
            "no info values to merge");

    switch (Values[0]->Kind) {
    case InfoKind::Namespace:
        return reduce<NamespaceInfo>(Values);
    case InfoKind::Record:
        return reduce<RecordInfo>(Values);
    case InfoKind::Enum:
        return reduce<EnumInfo>(Values);
    case InfoKind::Function:
        return reduce<FunctionInfo>(Values);
    case InfoKind::Typedef:
        return reduce<TypedefInfo>(Values);
    case InfoKind::Variable:
        return reduce<VarInfo>(Values);
    case InfoKind::Field:
        return reduce<FieldInfo>(Values);
    case InfoKind::Specialization:
        return reduce<SpecializationInfo>(Values);
    default:
        return llvm::createStringError(llvm::inconvertibleErrorCode(),
            "unexpected info type");
    }
}

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

mrdox::Expected<std::unique_ptr<Corpus>>
CorpusImpl::
build(
    tooling::ToolExecutor& ex,
    std::shared_ptr<Config const> config_)
{
    auto config = std::dynamic_pointer_cast<ConfigImpl const>(config_);
    auto corpus = std::make_unique<CorpusImpl>(config);

    // Traverse the AST for all translation units
    // and emit serializd bitcode into tool results.
    // This operation happens ona thread pool.
    if(corpus->config.verboseOutput)
        reportInfo("Mapping declarations");
    if(auto err = ex.execute(
        makeFrontendActionFactory(
            *ex.getExecutionContext(), *config)))
    {
        if(! corpus->config.ignoreFailures)
            return toError(std::move(err));
        reportWarning("warning: mapping failed because ", toString(std::move(err)));
    }

    // Inject the global namespace
    {
        // default-constructed NamespaceInfo
        // describes the global namespace
        NamespaceInfo I;
        insertBitcode(
            *ex.getExecutionContext(),
            writeBitcode(I));
    }

    // Collect the symbols. Each symbol will have
    // a vector of one or more bitcodes. These will
    // be merged later.
    if(corpus->config.verboseOutput)
        reportInfo("Collecting symbols");
    auto bitcodes = collectBitcodes(ex);

    // First reducing phase (reduce all decls into one info per decl).
    if(corpus->config.verboseOutput)
        reportInfo("Reducing {} declarations", bitcodes.size());
    std::atomic<bool> GotFailure;
    GotFailure = false;
    corpus->config.parallelForEach(
        bitcodes,
        [&](auto& Group)
        {
            // One or more Info for the same symbol ID
            std::vector<std::unique_ptr<Info>> Infos;

            // Each Bitcode can have multiple Infos
            for (auto& bitcode : Group.getValue())
            {
                auto infos = readBitcode(bitcode);
                if(! infos)
                {
                    reportError(infos.getError(), "read bitcode");
                    GotFailure = true;
                    return;
                }
                std::move(
                    infos->begin(),
                    infos->end(),
                    std::back_inserter(Infos));
            }

            auto merged = mergeInfos(Infos);
            if(! merged)
            {
                reportError(toError(merged.takeError()), "merge metadata");
                GotFailure = true;
                return;
            }

            std::unique_ptr<Info> I(merged.get().release());
            Assert(Group.getKey() == I->id);
            corpus->insert(std::move(I));
        });

    if(corpus->config.verboseOutput)
        llvm::outs() << "Collected " << corpus->InfoMap.size() << " symbols.\n";

    if(GotFailure)
        return Error("multiple errors occurred");

    corpus->canonicalize();

    return corpus;
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
    switch(I.Kind)
    {
    case InfoKind::Namespace:
        return f.visit(static_cast<NamespaceInfo&>(I));
    case InfoKind::Record:
        return f.visit(static_cast<RecordInfo&>(I));
    case InfoKind::Function:
        return f.visit(static_cast<FunctionInfo&>(I));
    case InfoKind::Typedef:
        return f.visit(static_cast<TypedefInfo&>(I));
    case InfoKind::Enum:
        return f.visit(static_cast<EnumInfo&>(I));
    case InfoKind::Variable:
        return f.visit(static_cast<VarInfo&>(I));
    case InfoKind::Field:
        return f.visit(static_cast<FieldInfo&>(I));
    default:
        llvm_unreachable("wrong InfoKind for visit");
    }
}

void
CorpusImpl::
traverse(
    MutableVisitor& f,
    NamespaceInfo& I)
{
    for(auto const& id : I.Members)
        this->traverse(f, id);
    // KRYSTIAN FIXME: should we traverse specializations?
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
    // KRYSTIAN FIXME: should we traverse specializations?
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
        canonicalize(I.Members);
        // KRYSTIAN FIXME: should we canonicalize specializations?
        // we shouldn't canonicalize anything if we intend to
        // preserve declaration order.

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

    void canonicalize(std::vector<SymbolID>& list) noexcept
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
canonicalize()
{
    if(isCanonical_)
        return;
    if(config_->verboseOutput)
        reportInfo("Canonicalizing...");
    Canonicalizer cn(*this);
    traverse(cn, SymbolID::zero);
    std::string temp0;
    std::string temp1;
    llvm::sort(index_,
        [&](Info const* I0,
            Info const* I1)
        {
            return compareSymbolNames(
                getFullyQualifiedName(*I0, temp0),
                getFullyQualifiedName(*I1, temp1)) < 0;
        });
    isCanonical_ = true;
}

} // mrdox
} // clang
