//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_METADATA_FINALIZER_BASEMEMBERSFINALIZER_HPP
#define MRDOCS_LIB_METADATA_FINALIZER_BASEMEMBERSFINALIZER_HPP

#include "lib/Metadata/InfoSet.hpp"
#include "lib/CorpusImpl.hpp"

namespace clang::mrdocs {

/** Finalizes a set of Info.

    This removes any references to SymbolIDs
    which do not exist.

    References which should always be valid
    are not checked.
*/
class BaseMembersFinalizer
{
    CorpusImpl& corpus_;
    std::unordered_set<SymbolID> finalized_;

    void
    inheritBaseMembers(RecordInfo& I, RecordInfo const& B, AccessKind A);

    void
    inheritBaseMembers(
        SymbolID const& derivedId,
        RecordInterface& derived,
        RecordInterface const& base,
        AccessKind A);
    
    template <std::ranges::range Range>
    void
    inheritBaseMembers(
        SymbolID const& derivedId,
        RecordTranche& derived,
        RecordTranche const& base,
        Range allMembers);

    template <std::ranges::range Range>
    void
    inheritBaseMembers(
        SymbolID const& derivedId,
        std::vector<SymbolID>& derived,
        std::vector<SymbolID> const& base,
        Range allMembers);

    void
    finalizeRecords(std::vector<SymbolID> const& ids);

    void
    finalizeNamespaces(std::vector<SymbolID> const& ids);

public:
    BaseMembersFinalizer(
        CorpusImpl& corpus)
        : corpus_(corpus)
    {}

    void
    build()
    {
        Info* info = corpus_.find(SymbolID::global);
        MRDOCS_CHECK_OR(info);
        operator()(info->asNamespace());
    }

    void
    operator()(NamespaceInfo& I);

    void
    operator()(RecordInfo& I);

    void
    operator()(Info&) {}
};

} // clang::mrdocs

#endif
