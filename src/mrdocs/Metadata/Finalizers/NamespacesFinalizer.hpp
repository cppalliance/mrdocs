//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_METADATA_FINALIZERS_NAMESPACESFINALIZER_HPP
#define MRDOCS_LIB_METADATA_FINALIZERS_NAMESPACESFINALIZER_HPP

#include <mrdocs/Corpus.hpp>
#include <mrdocs/detail/Corpus.hpp>

namespace mrdocs {

/** Finalizes the namespaces in corpus.

    Namespaces might be removed or have
    their extraction mode updated depending
    on its members.
*/
class NamespacesFinalizer
{
    Corpus& corpus_;
    Config const& config_;
    std::unordered_set<SymbolID> finalized_;

public:
    NamespacesFinalizer(
        Corpus& corpus, Config const& config)
        : corpus_(corpus)
        , config_(config)
    {}

    void
    build()
    {
        Symbol* info = corpus_.find(SymbolID::global);
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
    operator()(NamespaceSymbol& I);
};

} // mrdocs

#endif // MRDOCS_LIB_METADATA_FINALIZERS_NAMESPACESFINALIZER_HPP
