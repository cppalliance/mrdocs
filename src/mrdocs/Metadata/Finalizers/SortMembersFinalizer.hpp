//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_METADATA_FINALIZERS_SORTMEMBERSFINALIZER_HPP
#define MRDOCS_LIB_METADATA_FINALIZERS_SORTMEMBERSFINALIZER_HPP

#include <mrdocs/Corpus.hpp>
#include <mrdocs/detail/Corpus.hpp>

namespace mrdocs {

/** Finalizes a set of Info.

    This removes any references to SymbolIDs
    which do not exist.

    References which should always be valid
    are not checked.
*/
class SortMembersFinalizer
{
    Corpus& corpus_;
    Config const& config_;

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
    SortMembersFinalizer(Corpus& corpus, Config const& config)
        : corpus_(corpus)
        , config_(config)
    {}

    void
    build()
    {
        Symbol* globalPtr = corpus_.find(SymbolID::global);
        MRDOCS_CHECK_OR(globalPtr);
        MRDOCS_ASSERT(globalPtr->isNamespace());
        operator()(globalPtr->asNamespace());
    }

    void
    operator()(NamespaceSymbol& I);

    void
    operator()(RecordSymbol& I);

    void
    operator()(OverloadsSymbol& I);

    void
    operator()(Symbol&) {}
};

} // mrdocs

#endif // MRDOCS_LIB_METADATA_FINALIZERS_SORTMEMBERSFINALIZER_HPP
