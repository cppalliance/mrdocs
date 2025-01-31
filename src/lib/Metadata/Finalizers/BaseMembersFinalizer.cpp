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
#include "lib/Support/NameParser.hpp"

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
    if (A == AccessKind::Public)
    {
        // When a class uses public member access specifier to derive from a
        // base, all public members of the base class are accessible as public
        // members of the derived class and all protected members of the base
        // class are accessible as protected members of the derived class.
        // Private members of the base are never accessible unless friended.
        inheritBaseMembers(derivedId, derived.Public, base.Public);
        inheritBaseMembers(derivedId, derived.Protected, base.Protected);
    }
    else if (A == AccessKind::Protected)
    {
        // When a class uses protected member access specifier to derive from a
        // base, all public and protected members of the base class are
        // accessible as protected members of the derived class (private members
        // of the base are never accessible unless friended).
        inheritBaseMembers(derivedId, derived.Protected, base.Public);
        inheritBaseMembers(derivedId, derived.Protected, base.Protected);
    }
    else if (A == AccessKind::Private && config_->privateMembers)
    {
        // When a class uses private member access specifier to derive from a
        // base, all public and protected members of the base class are
        // accessible as private members of the derived class (private members
        // of the base are never accessible unless friended).
        inheritBaseMembers(derivedId, derived.Private, base.Public);
        inheritBaseMembers(derivedId, derived.Private, base.Protected);
    }
}

void
BaseMembersFinalizer::
inheritBaseMembers(
    SymbolID const& derivedId,
    RecordTranche& derived,
    RecordTranche const& base)
{
    inheritBaseMembers(derivedId, derived.NamespaceAliases, base.NamespaceAliases);
    inheritBaseMembers(derivedId, derived.Typedefs, base.Typedefs);
    inheritBaseMembers(derivedId, derived.Records, base.Records);
    inheritBaseMembers(derivedId, derived.Enums, base.Enums);
    inheritBaseMembers(derivedId, derived.Functions, base.Functions);
    inheritBaseMembers(derivedId, derived.StaticFunctions, base.StaticFunctions);
    inheritBaseMembers(derivedId, derived.Variables, base.Variables);
    inheritBaseMembers(derivedId, derived.StaticVariables, base.StaticVariables);
    inheritBaseMembers(derivedId, derived.Concepts, base.Concepts);
    inheritBaseMembers(derivedId, derived.Guides, base.Guides);
    inheritBaseMembers(derivedId, derived.Friends, base.Friends);
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

void
BaseMembersFinalizer::
inheritBaseMembers(
    SymbolID const& derivedId,
    std::vector<SymbolID>& derived,
    std::vector<SymbolID> const& base)
{
    for (SymbolID const& otherID: base)
    {
        // Find the info from the base class
        auto idIt = std::ranges::find(derived, otherID);
        MRDOCS_CHECK_OR_CONTINUE(idIt == derived.end());
        auto otherInfoIt = info_.find(otherID);
        MRDOCS_CHECK_OR_CONTINUE(otherInfoIt != info_.end());
        Info& otherInfo = *otherInfoIt->get();

        // Check if derived class has a member that shadows the base member
        auto shadowIt = std::ranges::find_if(
            derived,
            [&](SymbolID const& id)
            {
                auto infoIt = info_.find(id);
                MRDOCS_CHECK_OR(infoIt != info_.end(), false);
                auto& info = *infoIt->get();
                MRDOCS_CHECK_OR(info.Kind == otherInfo.Kind, false);
                if (info.isFunction())
                {
                    // If it's a function, it's only a shadow if the signatures
                    // are the same
                    auto const& otherFunc = static_cast<FunctionInfo const&>(otherInfo);
                    auto const& func = static_cast<FunctionInfo const&>(info);
                    return
                        std::tie(func.Name, func.Params, func.Template) ==
                        std::tie(otherFunc.Name, otherFunc.Params, func.Template);
                }
                else
                {
                    // For other kinds of members, it's a shadow if the names
                    // are the same
                    return info.Name == otherInfo.Name;
                }
            });
        MRDOCS_CHECK_OR_CONTINUE(shadowIt == derived.end());

        // Not a shadow, so inherit the base member
        if (!shouldCopy(config_, otherInfo))
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
                fmt::format(
                    "{}-{}",
                    toBase16Str(otherCopy->Parent),
                    toBase16Str(otherInfo.id)));
            derived.push_back(otherCopy->id);
            // Get the extraction mode from the derived class
            if (otherCopy->Extraction == ExtractionMode::Dependency)
            {
                auto derivedInfoIt = info_.find(derivedId);
                MRDOCS_CHECK_OR_CONTINUE(derivedInfoIt != info_.end());
                Info const& derivedInfo = **derivedInfoIt;
                otherCopy->Extraction = derivedInfo.Extraction;
            }
            info_.insert(std::move(otherCopy));
        }
    }
}

void
BaseMembersFinalizer::
finalizeRecords(std::vector<SymbolID> const& ids)
{
    for (SymbolID const& id: ids)
    {
        auto infoIt = info_.find(id);
        MRDOCS_CHECK_OR_CONTINUE(infoIt != info_.end());
        auto& info = *infoIt;
        auto* record = dynamic_cast<RecordInfo*>(info.get());
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
        auto infoIt = info_.find(id);
        MRDOCS_CHECK_OR_CONTINUE(infoIt != info_.end());
        auto& info = *infoIt;
        auto* ns = dynamic_cast<NamespaceInfo*>(info.get());
        MRDOCS_CHECK_OR_CONTINUE(ns);
        operator()(*ns);
    }
}

void
BaseMembersFinalizer::
operator()(NamespaceInfo& I)
{
    report::trace("Extracting base members for namespace '{}'", I.Name);
    finalizeRecords(I.Members.Records);
    finalizeNamespaces(I.Members.Namespaces);
}

void
BaseMembersFinalizer::
operator()(RecordInfo& I)
{
    report::trace("Extracting base members for record '{}'", I.Name);
    MRDOCS_CHECK_OR(!finalized_.contains(I.id));
    for (BaseInfo const& baseI: I.Bases)
    {
        auto const* baseNameType = get<NamedTypeInfo const*>(baseI.Type);
        MRDOCS_CHECK_OR_CONTINUE(baseNameType);
        auto const* baseName = get<NameInfo const*>(baseNameType->Name);
        MRDOCS_CHECK_OR_CONTINUE(baseName);
        SymbolID const baseID = baseName->id;
        MRDOCS_CHECK_OR_CONTINUE(baseID);
        auto baseIt = info_.find(baseID);
        MRDOCS_CHECK_OR_CONTINUE(baseIt != info_.end());
        std::unique_ptr<Info> const& base = *baseIt;
        auto* baseRecord = dynamic_cast<RecordInfo*>(base.get());
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
