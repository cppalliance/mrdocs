//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_METADATA_FINALIZER_DERIVEDFINALIZER_HPP
#define MRDOCS_LIB_METADATA_FINALIZER_DERIVEDFINALIZER_HPP

#include "lib/CorpusImpl.hpp"
#include "lib/Metadata/InfoSet.hpp"
#include <utility>

namespace clang::mrdocs {

/** Finalizes a set of Info.

    This finalizer finds non-member functions
    for a record and populate the related
    field of the javadoc.
*/
class DerivedFinalizer
{
    CorpusImpl& corpus_;

public:
    DerivedFinalizer(CorpusImpl& corpus)
        : corpus_(corpus)
    {}

    void
    build();
};

} // clang::mrdocs

#endif
