//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_METADATA_FINALIZER_SORTMEMBERSFINALIZER_HPP
#define MRDOCS_LIB_METADATA_FINALIZER_SORTMEMBERSFINALIZER_HPP

#include "lib/Lib/InfoSet.hpp"
#include "lib/Lib/CorpusImpl.hpp"

namespace clang::mrdocs {

/** Finalizes a set of Info.

    This removes any references to SymbolIDs
    which do not exist.

    References which should always be valid
    are not checked.
*/
class SortMembersFinalizer
{
    CorpusImpl& corpus_;

    void
    sortMembers(std::vector<SymbolID>& ids);

    void
    sortMembers(RecordInterface& I);

    void
    sortMembers(RecordTranche& I);

    void
    sortMembers(NamespaceTranche& I);

    void
    sortNamespaceMembers(std::vector<SymbolID>& id);

    void
    sortRecordMembers(std::vector<SymbolID>& id);

    void
    sortOverloadMembers(std::vector<SymbolID>& id);

public:
    SortMembersFinalizer(CorpusImpl& corpus)
        : corpus_(corpus)
    {}

    void
    build()
    {
        Info* globalPtr = corpus_.find(SymbolID::global);
        MRDOCS_CHECK_OR(globalPtr);
        MRDOCS_ASSERT(globalPtr->isNamespace());
        operator()(globalPtr->asNamespace());
    }

    void
    operator()(NamespaceInfo& I);

    void
    operator()(RecordInfo& I);

    void
    operator()(OverloadsInfo& I);

    void
    operator()(Info&) {}
};

} // clang::mrdocs

#endif
