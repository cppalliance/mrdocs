//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "RecordsFinalizer.hpp"
#include <mrdocs/Support/Algorithm.hpp>
#include <mrdocs/Support/Report.hpp>

namespace clang::mrdocs {

namespace {

void
addMember(std::vector<SymbolID>& container, Info const& Member)
{
    if (!contains(container, Member.id))
    {
        container.push_back(Member.id);
    }
}

void
addMember(RecordTranche& T, Info const& Member)
{
    if (auto const* U = Member.asNamespaceAliasPtr())
    {
        addMember(T.NamespaceAliases, *U);
        return;
    }
    if (auto const* U = Member.asTypedefPtr())
    {
        addMember(T.Typedefs, *U);
        return;
    }
    if (auto const* U = Member.asRecordPtr())
    {
        addMember(T.Records, *U);
        return;
    }
    if (auto const* U = Member.asEnumPtr())
    {
        addMember(T.Enums, *U);
        return;
    }
    if (auto const* U = Member.asFunctionPtr())
    {
        if (U->StorageClass != StorageClassKind::Static)
        {
            addMember(T.Functions, *U);
        }
        else
        {
            addMember(T.StaticFunctions, *U);
        }
        return;
    }
    if (auto const* U = Member.asVariablePtr())
    {
        if (U->StorageClass != StorageClassKind::Static)
        {
            addMember(T.Variables, *U);
        }
        else
        {
            addMember(T.StaticVariables, *U);
        }
        return;
    }
    if (auto const* U = Member.asConceptPtr())
    {
        addMember(T.Concepts, *U);
        return;
    }
    if (auto const* U = Member.asGuidePtr())
    {
        addMember(T.Guides, *U);
        return;
    }
    if (auto const* U = Member.asUsingPtr())
    {
        addMember(T.Usings, *U);
        return;
    }
    if (auto const* U = Member.asOverloadsPtr())
    {
        addMember(T.Functions, *U);
        return;
    }
    report::error("Cannot push {} of type {} into tranche",
        Member.Name,
        mrdocs::toString(Member.Kind).c_str());
}

}

void
RecordsFinalizer::
generateRecordInterface(RecordInfo& I)
{
    for (auto const& m: I.Members)
    {
        Info* infoPtr = corpus_.find(m.id);
        MRDOCS_CHECK_OR_CONTINUE(infoPtr);

        Info const& Member = *infoPtr;
        switch (Member.Access)
        {
        case AccessKind::Public:
            addMember(I.Interface.Public, Member);
            break;
        case AccessKind::Private:
            addMember(I.Interface.Private, Member);
            break;
        case AccessKind::Protected:
            addMember(I.Interface.Protected, Member);
            break;
        default:
            MRDOCS_UNREACHABLE();
        }
    }
}

void
RecordsFinalizer::
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
RecordsFinalizer::
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
RecordsFinalizer::
operator()(NamespaceInfo& I)
{
    report::trace(
        "Generating record interfaces for namespace '{}'",
        corpus_.Corpus::qualifiedName(I));
    finalizeRecords(I.Members.Records);
    finalizeNamespaces(I.Members.Namespaces);
}

void
RecordsFinalizer::
operator()(RecordInfo& I)
{
    report::trace(
        "Generating record interface for record '{}'",
        corpus_.Corpus::qualifiedName(I));
    MRDOCS_CHECK_OR(!finalized_.contains(I.id));
    generateRecordInterface(I);
    finalizeRecords(I.Interface.Public.Records);
    finalizeRecords(I.Interface.Protected.Records);
    finalizeRecords(I.Interface.Private.Records);
    finalized_.emplace(I.id);
}

} // clang::mrdocs
