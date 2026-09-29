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
#include <vector>

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

    // `baseId` is the base as written in the base-specifier: the primary
    // template for a specialization. Copied members record it as the class
    // they are inherited from.
    void
    inheritBaseMembers(
        RecordSymbol& I,
        RecordSymbol const& B,
        SymbolID const& baseId,
        AccessKind A,
        SourceInfo const& baseLoc);

    void
    inheritBaseMembers(
        SymbolID const& derivedId,
        SymbolID const& baseId,
        RecordInterface& derived,
        RecordInterface const& base,
        AccessKind A,
        SourceInfo const& baseLoc);

    void
    inheritBaseMembers(
        SymbolID const& derivedId,
        SymbolID const& baseId,
        RecordTranche& derived,
        RecordTranche const& base,
        SourceInfo const& baseLoc);

    void
    inheritBaseMembers(
        SymbolID const& derivedId,
        SymbolID const& baseId,
        std::vector<SymbolID>& derived,
        std::vector<SymbolID> const& base,
        std::unordered_set<std::string> const& derivedNames,
        SourceInfo const& baseLoc);

    // The names the members of a tranche go by.
    std::unordered_set<std::string>
    memberNames(RecordTranche const& T) const;

public:
    BaseMembersFinalizer(
        Corpus& corpus, Config const& config)
        : corpus_(corpus)
        , config_(config)
    {}

    /** The ids of every record in the corpus, in source order.

        A snapshot to iterate while symbols are inserted into the corpus,
        ordered by definition location and then by id so the output does
        not depend on the corpus's iteration order. Shared with
        `HiddenFriendsFinalizer`.
    */
    static
    std::vector<SymbolID>
    recordsInSourceOrder(Corpus const& corpus);


    void
    build()
    {
        // Every record is visited from a snapshot of the corpus, since
        // copies are inserted while inheriting; the snapshot is in source
        // order, so a derived class is handled after the ones declared
        // before it and the output stays deterministic.
        std::vector<SymbolID> const records = recordsInSourceOrder(corpus_);
        for (SymbolID const& id : records)
        {
            Symbol* symbol = corpus_.find(id);
            MRDOCS_CHECK_OR_CONTINUE(symbol && symbol->isRecord());
            operator()(symbol->asRecord());
        }
    }

    void
    operator()(RecordSymbol& I);

};

} // mrdocs

#endif // MRDOCS_LIB_METADATA_FINALIZERS_BASEMEMBERSFINALIZER_HPP
