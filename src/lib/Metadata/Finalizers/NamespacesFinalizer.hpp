//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_METADATA_FINALIZER_NAMESPACESFINALIZER_HPP
#define MRDOCS_LIB_METADATA_FINALIZER_NAMESPACESFINALIZER_HPP

#include "lib/Lib/Info.hpp"
#include "lib/Lib/CorpusImpl.hpp"

namespace clang::mrdocs {

/** Finalizes a set of Info.

    This removes any references to SymbolIDs
    which do not exist.

    References which should always be valid
    are not checked.
*/
class NamespacesFinalizer
{
    CorpusImpl& corpus_;
    std::unordered_set<SymbolID> finalized_;

public:
    NamespacesFinalizer(
        CorpusImpl& corpus)
        : corpus_(corpus)
    {}

    void
    build()
    {
        Info* info = corpus_.find(SymbolID::global);
        MRDOCS_CHECK_OR(info);
        operator()(*dynamic_cast<NamespaceInfo*>(info));
    }

    enum class FinalizerResult {
        None,
        Removed,
        Changed
    };

    FinalizerResult
    operator()(NamespaceInfo& I);
};

} // clang::mrdocs

#endif
