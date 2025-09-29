//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "NamespacesFinalizer.hpp"
#include <mrdocs/Support/Report.hpp>

namespace mrdocs {

NamespacesFinalizer::FinalizerResult
NamespacesFinalizer::
operator()(NamespaceSymbol& I)
{
    report::trace(
        "Finalizing namespace '{}'",
        corpus_.Corpus::qualifiedName(I));

    // 1) Finalize sub-namespaces
    llvm::SmallVector<SymbolID, 64> removedNamespaces;
    for (auto subNamespaces = corpus_.find(I.Members.Namespaces);
         Symbol& info: subNamespaces)
    {
        MRDOCS_ASSERT(info.isNamespace());
        SymbolID memberID = info.id;
        auto& member = info.asNamespace();
        if (auto const res = (*this)(member);
            res == FinalizerResult::Removed)
        {
            removedNamespaces.push_back(memberID);
        }
    }
    for (SymbolID const& memberID : removedNamespaces)
    {
        auto it = std::ranges::find(I.Members.Namespaces, memberID);
        MRDOCS_ASSERT(it != I.Members.Namespaces.end());
        I.Members.Namespaces.erase(it);
    }

    // No more steps for the global namespace
    MRDOCS_CHECK_OR(I.id != SymbolID::global, FinalizerResult::None);

    // No more steps for documented namespaces
    MRDOCS_CHECK_OR(!I.doc, FinalizerResult::None);

    // 3) Remove empty undocumented namespaces
    auto memberIds = allMembers(I);
    if (std::ranges::empty(memberIds))
    {
        if (!corpus_.config->extractEmptyNamespaces)
        {
            auto const it = corpus_.info_.find(I.id);
            MRDOCS_CHECK_OR(it != corpus_.info_.end(), FinalizerResult::None);
            corpus_.info_.erase(it);
            return FinalizerResult::Removed;
        }
        return FinalizerResult::None;
    }

    // 4) If the namespace is regular and undocumented,
    // it's extraction mode is updated according to
    // its members
    MRDOCS_CHECK_OR(I.Extraction == ExtractionMode::Regular, FinalizerResult::None);
    auto members = corpus_.find(memberIds);
    bool allDependencies = true;
    bool allImplementationDefined = true;
    bool anySeeBelow = false;
    for (Symbol const& member : members)
    {
        if (member.Extraction == ExtractionMode::Regular)
        {
            // It has a regular member, so it should remain
            // regular
            return FinalizerResult::None;
        }
        if (member.Extraction != ExtractionMode::Dependency)
        {
            allDependencies = false;
        }
        if (member.Extraction != ExtractionMode::ImplementationDefined)
        {
            allImplementationDefined = false;
        }
        if (member.Extraction == ExtractionMode::SeeBelow)
        {
            anySeeBelow = true;
        }
    }

    if (allDependencies)
    {
        I.Extraction = ExtractionMode::Dependency;
    }
    else if (allImplementationDefined)
    {
        I.Extraction = ExtractionMode::ImplementationDefined;
    }
    else if (anySeeBelow)
    {
        I.Extraction = ExtractionMode::SeeBelow;
    }
    return FinalizerResult::Changed;
}

} // mrdocs
