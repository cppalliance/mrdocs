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

#include "lib/Lib/Info.hpp"
#include "lib/Lib/Lookup.hpp"

namespace clang::mrdocs {

/** Finalizes a set of Info.

    This removes any references to SymbolIDs
    which do not exist.

    References which should always be valid
    are not checked.
*/
class SortMembersFinalizer
{
    InfoSet& info_;
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
    SortMembersFinalizer(InfoSet& Info, Config const& config)
        : info_(Info)
        , config_(config)
    {}

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
