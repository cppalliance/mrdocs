//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "Finalize.hpp"
#include "lib/Metadata/Finalizers/BaseMembersFinalizer.hpp"
#include "lib/Metadata/Finalizers/OverloadsFinalizer.hpp"
#include "lib/Metadata/Finalizers/ReferenceFinalizer.hpp"
#include "lib/Metadata/Finalizers/SortMembersFinalizer.hpp"
#include "lib/Lib/Info.hpp"
#include <mrdocs/Metadata.hpp>

namespace clang::mrdocs {

namespace {
void
finalizeBaseMembers(InfoSet& Info, Config const& config)
{
    MRDOCS_CHECK_OR(config->inheritBaseMembers != PublicSettings::BaseMemberInheritance::Never);
    BaseMembersFinalizer baseMembersFinalizer(Info, config);
    auto const globalIt = Info.find(SymbolID::global);
    MRDOCS_CHECK_OR(globalIt != Info.end());
    baseMembersFinalizer(*dynamic_cast<NamespaceInfo*>(globalIt->get()));
}

void
finalizeOverloads(InfoSet& Info, Config const& config)
{
    MRDOCS_CHECK_OR(config->overloads);
    OverloadsFinalizer baseMembersFinalizer(Info);
    auto const globalIt = Info.find(SymbolID::global);
    MRDOCS_CHECK_OR(globalIt != Info.end());
    baseMembersFinalizer(*dynamic_cast<NamespaceInfo*>(globalIt->get()));
}

void
finalizeMemberOrder(InfoSet& Info, Config const& config)
{
    MRDOCS_CHECK_OR(config->sortMembers);
    SortMembersFinalizer sortMembersFinalizer(Info, config);
    auto const globalIt = Info.find(SymbolID::global);
    MRDOCS_CHECK_OR(globalIt != Info.end());
    sortMembersFinalizer(*dynamic_cast<NamespaceInfo*>(globalIt->get()));
}

void
finalizeReferences(InfoSet& Info, SymbolLookup& Lookup)
{
    ReferenceFinalizer visitor(Info, Lookup);
    for (auto& I : Info)
    {
        MRDOCS_ASSERT(I);
        visitor.finalize(*I);
    }
}
}

/** Finalizes a set of Info.

    This removes any references to SymbolIDs
    which do not exist.

    References which should always be valid are not checked.
*/
void
finalize(CorpusImpl& corpus)
{
    finalizeBaseMembers(corpus.info_, *corpus.config_);
    finalizeOverloads(corpus.info_, *corpus.config_);
    finalizeMemberOrder(corpus.info_, *corpus.config_);
    auto const lookup = std::make_unique<SymbolLookup>(corpus);
    finalizeReferences(corpus.info_, *lookup);
}

} // clang::mrdocs
