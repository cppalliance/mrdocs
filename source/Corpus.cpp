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
#include "ConfigImpl.hpp"
#include "AST/Bitcode.hpp"
#include "AST/FrontendAction.hpp"
#include "Metadata/Reduce.hpp"
#include "Support/Error.hpp"
#include <mrdox/Support/Report.hpp>
#include <mrdox/Metadata.hpp>
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <llvm/Bitstream/BitstreamReader.h>
#include <llvm/Support/Mutex.h>
#include <cassert>

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
    default:
        return llvm::createStringError(llvm::inconvertibleErrorCode(),
            "unexpected info type");
    }
}

//------------------------------------------------

Corpus::~Corpus() noexcept = default;

//------------------------------------------------
//
// Observers
//
//------------------------------------------------

/** Return the metadata for the global namespace.
*/
NamespaceInfo const&
Corpus::
globalNamespace() const noexcept
{
    return get<NamespaceInfo>(SymbolID::zero);
}

//------------------------------------------------

Corpus::
Visitor::
~Visitor() noexcept = default;

bool Corpus::Visitor::visit(NamespaceInfo const&)
{
    return true;
}

bool Corpus::Visitor::visit(RecordInfo const&)
{
    return true;
}

bool Corpus::Visitor::visit(FunctionInfo const&)
{
    return true;
}

bool Corpus::Visitor::visit(TypedefInfo const&)
{
    return true;
}

bool Corpus::Visitor::visit(EnumInfo const&)
{
    return true;
}

bool Corpus::Visitor::visit(VarInfo const&)
{
    return true;
}

bool Corpus::Visitor::visit(FieldInfo const&)
{
    return true;
}

//---

bool Corpus::Visitor::visit(MemberEnum const& I, Access)
{
    return true;
}

bool Corpus::Visitor::visit(MemberFunction const& I, Access)
{
    return true;
}

bool Corpus::Visitor::visit(MemberRecord const& I, Access)
{
    return true;
}

bool Corpus::Visitor::visit(MemberType const& I, Access)
{
    return true;
}

bool Corpus::Visitor::visit(DataMember const&, Access)
{
    return true;
}

bool Corpus::Visitor::visit(StaticDataMember const& I, Access)
{
    return true;
}


//------------------------------------------------

bool
Corpus::
traverse(
    Visitor& f,
    Info const& I) const
{
    switch(I.Kind)
    {
    case InfoKind::Namespace:
        return f.visit(static_cast<NamespaceInfo const&>(I));
    case InfoKind::Record:
        return f.visit(static_cast<RecordInfo const&>(I));
    case InfoKind::Function:
        return f.visit(static_cast<FunctionInfo const&>(I));
    case InfoKind::Typedef:
        return f.visit(static_cast<TypedefInfo const&>(I));
    case InfoKind::Enum:
        return f.visit(static_cast<EnumInfo const&>(I));
    case InfoKind::Variable:
        return f.visit(static_cast<VarInfo const&>(I));
    // KRYSTIAN FIXME: is this correct?
    case InfoKind::Field:
        return f.visit(static_cast<FieldInfo const&>(I));
    default:
        llvm_unreachable("wrong InfoKind for visit");
    }
}

bool
Corpus::
traverse(
    Visitor& f,
    NamespaceInfo const& I) const
{
    for(auto const& ref : I.Children.Namespaces)
        if(! f.visit(get<NamespaceInfo>(ref.id)))
            return false;
    for(auto const& ref : I.Children.Records)
        if(! f.visit(get<RecordInfo>(ref.id)))
            return false;
    for(auto const& ref : I.Children.Functions)
        if(! f.visit(get<FunctionInfo>(ref.id)))
            return false;
    for(auto const& ref : I.Children.Typedefs)
        if(! f.visit(get<TypedefInfo>(ref.id)))
            return false;
    for(auto const& ref : I.Children.Enums)
        if(! f.visit(get<EnumInfo>(ref.id)))
            return false;
    for(auto const& ref : I.Children.Vars)
        if(! f.visit(get<VarInfo>(ref.id)))
            return false;
    return true;
}

bool
Corpus::
traverse(
    Visitor& f,
    RecordInfo const& I) const
{
    for(auto const& t : I.Members.Records)
        if(! f.visit(MemberRecord{
            &get<RecordInfo>(t.id),
                &I}, t.access))
            return false;
    for(auto const& t : I.Members.Functions)
        if(! f.visit(MemberFunction{
            &get<FunctionInfo>(t.id),
                &I}, t.access))
            return false;
    for(auto const& t : I.Members.Types)
        if(! f.visit(MemberType{
            &get<TypedefInfo>(t.id),
                &I}, t.access))
            return false;
    for(auto const& t : I.Members.Enums)
        if(! f.visit(MemberEnum{
            &get<EnumInfo>(t.id),
                &I}, t.access))
            return false;
    for(auto const& t : I.Members.Fields)
        if(! f.visit(DataMember{
            &get<FieldInfo>(t.id),
                &I}, t.access))
            return false;
    for(auto const& t : I.Members.Vars)
        if(! f.visit(StaticDataMember{
            &get<VarInfo>(t.id),
                &I}, t.access))
            return false;

    return true;
}

bool
Corpus::
traverse(Visitor& f, SymbolID id) const
{
    return traverse(f, get<Info>(id));
}

bool
Corpus::
traverse(
    Visitor& f,
    std::vector<Reference> const& R) const
{
    for(auto const& ref : R)
        if(! traverse(f, get<Info>(ref.id)))
            return false;
    return true;
}

bool
Corpus::
traverse(
    Visitor& f,
    std::vector<SymbolID> const& R) const
{
    for(auto const& id : R)
        if(! traverse(f, get<Info>(id)))
            return false;
    return true;
}

//------------------------------------------------
//
// Modifiers
//
//------------------------------------------------

Expected<std::unique_ptr<Corpus>>
Corpus::
build(
    tooling::ToolExecutor& ex,
    std::shared_ptr<Config const> config_)
{
    auto config = std::dynamic_pointer_cast<ConfigImpl const>(config_);
    auto corpus = std::make_unique<CorpusImpl>(config);

    // Build arguments adjuster
    tooling::ArgumentsAdjuster ArgAdjuster;
    {
        for(auto const& define : config->additionalDefines_)
        {
            std::string s;
            llvm::raw_string_ostream os(s);
            os << "-D" << define;
            ArgAdjuster = tooling::combineAdjusters(
                tooling::getInsertArgumentAdjuster(
                    s.c_str(), tooling::ArgumentInsertPosition::END),
                ArgAdjuster);
        }
    }

    // Traverse the AST for all translation units
    // and emit serializd bitcode into tool results.
    // This operation happens ona thread pool.
    if(corpus->config.verboseOutput)
        reportInfo("Mapping declarations");
    if(auto err = ex.execute(
        makeFrontendActionFactory(
            *ex.getExecutionContext(), *config), ArgAdjuster))
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

// KRYSTIAN NOTE: temporary
std::string&
Corpus::
getFullyQualifiedName(
    const Info& I,
    std::string& temp) const
{
    temp.clear();
    for(auto const& ns_id : llvm::reverse(I.Namespace))
    {
        if(const Info* ns = find(ns_id))
            temp.append(ns->Name.data(), ns->Name.size());
        else
            temp.append("<unnamed>");

        temp.append("::");
    }
    auto s = I.extractName();
    temp.append(s.data(), s.size());
    return temp;
}


} // mrdox
} // clang
