//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_METADATA_FINALIZER_RECORDSFINALIZER_HPP
#define MRDOCS_LIB_METADATA_FINALIZER_RECORDSFINALIZER_HPP

#include "lib/Metadata/InfoSet.hpp"
#include "lib/CorpusImpl.hpp"

namespace clang::mrdocs {

/** Finalizes the records in corpus.

    Generates record interfaces.
*/
class RecordsFinalizer
{
    CorpusImpl& corpus_;
    std::unordered_set<SymbolID> finalized_;

    void
    generateRecordInterface(RecordInfo& I);

    void
    finalizeRecords(std::vector<SymbolID> const& ids);

    void
    finalizeNamespaces(std::vector<SymbolID> const& ids);

public:
    RecordsFinalizer(
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
