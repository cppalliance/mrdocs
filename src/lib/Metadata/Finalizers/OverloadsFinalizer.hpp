//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_METADATA_FINALIZERS_OVERLOADSFINALIZER_HPP
#define MRDOCS_LIB_METADATA_FINALIZERS_OVERLOADSFINALIZER_HPP

#include <lib/CorpusImpl.hpp>
#include <lib/Metadata/SymbolSet.hpp>

namespace mrdocs {

/** Finalizes a set of Info.

    This removes any references to SymbolIDs
    which do not exist.

    References which should always be valid
    are not checked.
*/
class OverloadsFinalizer
{
    CorpusImpl& corpus_;
    std::set<SymbolID> finalized_;

    void
    foldOverloads(
        SymbolID const& contextId,
        std::vector<SymbolID>& functionIds,
        bool isStatic);

public:
    OverloadsFinalizer(CorpusImpl& corpus)
        : corpus_(corpus)
    {}

    void
    build()
    {
        auto const globalPtr = corpus_.find(SymbolID::global);
        MRDOCS_CHECK_OR(globalPtr);
        MRDOCS_ASSERT(globalPtr->isNamespace());
        operator()(globalPtr->asNamespace());
    }

    /* Visit the namespace members identifying overload sets
     */
    void
    operator()(NamespaceSymbol& I);

    /* Visit the record members identifying overload sets
     */
    void
    operator()(RecordSymbol& I);

    /* Visit the using directive shadows for overloads
     */
    void
    operator()(UsingSymbol& I);

    /* No-op for other Info types
     */
    void
    operator()(Symbol&) {}
};

} // mrdocs

#endif // MRDOCS_LIB_METADATA_FINALIZERS_OVERLOADSFINALIZER_HPP
