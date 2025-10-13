//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "BaseMembersFinalizer.hpp"
#include <format>
#include <mrdocs/Support/Algorithm.hpp>
#include <mrdocs/Support/Report.hpp>

namespace clang::mrdocs {

void
BaseMembersFinalizer::
inheritBaseMembers(RecordInfo& I, RecordInfo const& B, AccessKind const A)
{
    inheritBaseMembers(I.id, I.Interface, B.Interface, A);
}

void
BaseMembersFinalizer::
inheritBaseMembers(
    SymbolID const& derivedId,
    RecordInterface& derived,
    RecordInterface const& base,
    AccessKind const A)
{
    auto members = allMembers(derived);

    if (A == AccessKind::Public)
    {
        // When a class uses public member access specifier to derive from a
        // base, all public members of the base class are accessible as public
        // members of the derived class and all protected members of the base
        // class are accessible as protected members of the derived class.
        // Private members of the base are never accessible unless friended.
        inheritBaseMembers(derivedId, derived.Public, base.Public, members);
        inheritBaseMembers(derivedId, derived.Protected, base.Protected, members);
    }
    else if (A == AccessKind::Protected)
    {
        // When a class uses protected member access specifier to derive from a
        // base, all public and protected members of the base class are
        // accessible as protected members of the derived class (private members
        // of the base are never accessible unless friended).
        inheritBaseMembers(derivedId, derived.Protected, base.Public, members);
        inheritBaseMembers(derivedId, derived.Protected, base.Protected, members);
    }
    else if (A == AccessKind::Private && corpus_.config->extractPrivate)
    {
        // When a class uses private member access specifier to derive from a
        // base, all public and protected members of the base class are
        // accessible as private members of the derived class (private members
        // of the base are never accessible unless friended).
        inheritBaseMembers(derivedId, derived.Private, base.Public, members);
        inheritBaseMembers(derivedId, derived.Private, base.Protected, members);
    }
}

template <std::ranges::range Range>
void
BaseMembersFinalizer::
inheritBaseMembers(
    SymbolID const& derivedId,
    RecordTranche& derived,
    RecordTranche const& base,
    Range allMembers)
{
    inheritBaseMembers(derivedId, derived.NamespaceAliases, base.NamespaceAliases, allMembers);
    inheritBaseMembers(derivedId, derived.Typedefs, base.Typedefs, allMembers);
    inheritBaseMembers(derivedId, derived.Records, base.Records, allMembers);
    inheritBaseMembers(derivedId, derived.Enums, base.Enums, allMembers);
    inheritBaseMembers(derivedId, derived.Functions, base.Functions, allMembers);
    inheritBaseMembers(derivedId, derived.StaticFunctions, base.StaticFunctions, allMembers);
    inheritBaseMembers(derivedId, derived.Variables, base.Variables, allMembers);
    inheritBaseMembers(derivedId, derived.StaticVariables, base.StaticVariables, allMembers);
    inheritBaseMembers(derivedId, derived.Concepts, base.Concepts, allMembers);
    inheritBaseMembers(derivedId, derived.Guides, base.Guides, allMembers);
}

namespace {
bool
shouldCopy(Config const& config, Info const& M)
{
    if (config->inheritBaseMembers == PublicSettings::BaseMemberInheritance::CopyDependencies)
    {
        return M.Extraction == ExtractionMode::Dependency;
    }
    return config->inheritBaseMembers == PublicSettings::BaseMemberInheritance::CopyAll;
}
}

template <std::ranges::range Range>
void
BaseMembersFinalizer::
inheritBaseMembers(
    SymbolID const& derivedId,
    std::vector<SymbolID>& derived,
    std::vector<SymbolID> const& base,
    Range allMembers)
{
    for (SymbolID const& otherID: base)
    {
        // Find the info from the base class
        MRDOCS_CHECK_OR_CONTINUE(!contains(derived, otherID));
        Info* otherInfoPtr = corpus_.find(otherID);
        MRDOCS_CHECK_OR_CONTINUE(otherInfoPtr);
        Info& otherInfo = *otherInfoPtr;

        // Check if we're not attempt to copy a special member function
        if (auto const *funcPtr =
                dynamic_cast<FunctionInfo const *>(otherInfoPtr)) {
          MRDOCS_CHECK_OR_CONTINUE(
              !is_one_of(funcPtr->Class, {FunctionClass::Constructor,
                                          FunctionClass::Destructor}));
        }

        // Check if derived class has a member that shadows the base member
        auto shadowIt = std::ranges::find_if(
            allMembers,
            [&](SymbolID const& id)
            {
                Info* infoPtr = corpus_.find(id);
                MRDOCS_CHECK_OR(infoPtr, false);
                auto& info = *infoPtr;
                MRDOCS_CHECK_OR(info.Kind == otherInfo.Kind, false);
                if (info.isFunction())
                {
                    // If it's a function, it's only a shadow if the signatures
                    // are the same
                    auto const& otherFunc = static_cast<FunctionInfo const&>(otherInfo);
                    auto const& func = static_cast<FunctionInfo const&>(info);
                    return overrides(func, otherFunc);
                }
                // For other kinds of members, it's a shadow if the names
                // are the same
                return info.Name == otherInfo.Name;
            });
        MRDOCS_CHECK_OR_CONTINUE(shadowIt == allMembers.end());

        // Not a shadow, so inherit the base member
        if (!shouldCopy(corpus_.config, otherInfo))
        {
            // When it's a dependency, we don't create a reference to
            // the member because the reference would be invalid.
            // The user can use `copy-dependencies` or `copy` to
            // copy the dependencies.
            // There could be another option that forces the symbol
            // extraction mode to be regular, but that is controversial.
            if (otherInfo.Extraction != ExtractionMode::Dependency)
            {
                derived.push_back(otherID);
            }
        }
        else
        {
            std::unique_ptr<Info> otherCopy =
                visit(otherInfo, [&]<class T>(T const& other)
                    -> std::unique_ptr<Info>
                {
                    return std::make_unique<T>(other);
                });
            otherCopy->Parent = derivedId;
            otherCopy->id = SymbolID::createFromString(
                std::format("{}-{}", toBase16Str(otherCopy->Parent),
                            toBase16Str(otherInfo.id)));
            derived.push_back(otherCopy->id);
            // Get the extraction mode from the derived class
            if (otherCopy->Extraction == ExtractionMode::Dependency)
            {
                Info* derivedInfoPtr = corpus_.find(derivedId);
                MRDOCS_CHECK_OR_CONTINUE(derivedInfoPtr);
                Info const& derivedInfo = *derivedInfoPtr;
                otherCopy->Extraction = derivedInfo.Extraction;
            }
            corpus_.info_.insert(std::move(otherCopy));
        }
    }
}

void
BaseMembersFinalizer::
finalizeRecords(std::vector<SymbolID> const& ids)
{
    for (SymbolID const& id: ids)
    {
        Info* infoPtr = corpus_.find(id);
        MRDOCS_CHECK_OR_CONTINUE(infoPtr);
        auto* record = infoPtr->asRecordPtr();
        MRDOCS_CHECK_OR_CONTINUE(record);
        operator()(*record);
    }
}

void
BaseMembersFinalizer::
finalizeNamespaces(std::vector<SymbolID> const& ids)
{
    for (SymbolID const& id: ids)
    {
        Info* infoPtr = corpus_.find(id);
        MRDOCS_CHECK_OR_CONTINUE(infoPtr);
        auto* ns = infoPtr->asNamespacePtr();
        MRDOCS_CHECK_OR_CONTINUE(ns);
        operator()(*ns);
    }
}

void
BaseMembersFinalizer::
operator()(NamespaceInfo& I)
{
    report::trace(
        "Extracting base members for namespace '{}'",
        corpus_.Corpus::qualifiedName(I));
    finalizeRecords(I.Members.Records);
    finalizeNamespaces(I.Members.Namespaces);
}

void
BaseMembersFinalizer::
operator()(RecordInfo& I)
{
    MRDOCS_CHECK_OR(I.Extraction == ExtractionMode::Regular);
    report::trace(
        "Extracting base members for record '{}'",
        corpus_.Corpus::qualifiedName(I));
    MRDOCS_CHECK_OR(!finalized_.contains(I.id));
    for (BaseInfo const& baseI: I.Bases)
    {
        auto const *baseNameType =
            dynamic_cast<NamedTypeInfo const *>(&*baseI.Type);
        MRDOCS_CHECK_OR_CONTINUE(baseNameType);
        auto const *baseName =
            dynamic_cast<NameInfo const *>(&*baseNameType->Name);
        MRDOCS_CHECK_OR_CONTINUE(baseName);
        SymbolID baseID = baseName->id;
        if (corpus_.config->extractImplicitSpecializations && 
            baseName->isSpecialization())
        {
            auto const *baseSpec =
                dynamic_cast<SpecializationNameInfo const *>(baseName);
            MRDOCS_CHECK_OR_CONTINUE(baseSpec);
            baseID = baseSpec->specializationID;
        }
        MRDOCS_CHECK_OR_CONTINUE(baseID);
        auto basePtr = corpus_.find(baseID);
        MRDOCS_CHECK_OR_CONTINUE(basePtr);
        auto* baseRecord = basePtr->asRecordPtr();
        MRDOCS_CHECK_OR_CONTINUE(baseRecord);
        operator()(*baseRecord);
        inheritBaseMembers(I, *baseRecord, baseI.Access);
    }
    finalizeRecords(I.Interface.Public.Records);
    finalizeRecords(I.Interface.Protected.Records);
    finalizeRecords(I.Interface.Private.Records);
    finalized_.emplace(I.id);
}

} // clang::mrdocs
