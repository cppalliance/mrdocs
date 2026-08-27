//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_METADATA_FINALIZERS_BASEMEMBERSFINALIZER_HPP
#define MRDOCS_LIB_METADATA_FINALIZERS_BASEMEMBERSFINALIZER_HPP

#include <mrdocs/Corpus.hpp>
#include <mrdocs/detail/Corpus.hpp>

namespace mrdocs {

/** Finalizes a set of Info.

    This removes any references to SymbolIDs
    which do not exist.

    References which should always be valid
    are not checked.
*/
class BaseMembersFinalizer
{
    Corpus& corpus_;
    Config const& config_;
    std::unordered_set<SymbolID> finalized_;

    void
    inheritBaseMembers(RecordSymbol& I, RecordSymbol const& B, AccessKind A);

    void
    inheritBaseMembers(
        SymbolID const& derivedId,
        RecordInterface& derived,
        RecordInterface const& base,
        AccessKind A);

    void
    inheritBaseMembers(
        SymbolID const& derivedId,
        RecordTranche& derived,
        RecordTranche const& base);

    void
    inheritBaseMembers(
        SymbolID const& derivedId,
        std::vector<SymbolID>& derived,
        std::vector<SymbolID> const& base,
        std::unordered_set<std::string> const& derivedNames);

    // The names the members of a tranche go by.
    std::unordered_set<std::string>
    memberNames(RecordTranche const& T) const;

    void
    finalizeRecords(std::vector<SymbolID> const& ids);

    void
    finalizeNamespaces(std::vector<SymbolID> const& ids);

public:
    BaseMembersFinalizer(
        Corpus& corpus, Config const& config)
        : corpus_(corpus)
        , config_(config)
    {}

    void
    build()
    {
        Symbol* info = corpus_.find(SymbolID::global);
        MRDOCS_CHECK_OR(info);
        operator()(info->asNamespace());
    }

    void
    operator()(NamespaceSymbol& I);

    void
    operator()(RecordSymbol& I);

    void
    operator()(Symbol&) {}
};

} // mrdocs

#endif // MRDOCS_LIB_METADATA_FINALIZERS_BASEMEMBERSFINALIZER_HPP
