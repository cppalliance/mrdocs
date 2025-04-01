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

#include "lib/Lib/InfoSet.hpp"
#include "lib/Lib/CorpusImpl.hpp"

namespace clang::mrdocs {

/** Finalizes the namespaces in corpus.

    Namespaces might be removed or have
    their extraction mode updated depending
    on its members.
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
        MRDOCS_ASSERT(info->isNamespace());
        operator()(info->asNamespace());
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
