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

#ifndef MRDOCS_LIB_METADATA_FINALIZERS_DERIVEDFINALIZER_HPP
#define MRDOCS_LIB_METADATA_FINALIZERS_DERIVEDFINALIZER_HPP

#include <mrdocs/Corpus.hpp>
#include <mrdocs/detail/Corpus.hpp>
#include <utility>

namespace mrdocs {

/** Finalizes a set of Info.

    This finalizer finds derived classes
    for each base class and populates the
    related `Derived` field of the RecordSymbol.
*/
class DerivedFinalizer
{
    Corpus& corpus_;
    Config const& config_;

public:
    DerivedFinalizer(Corpus& corpus, Config const& config)
        : corpus_(corpus)
        , config_(config)
    {}

    void
    build();
};

} // mrdocs

#endif // MRDOCS_LIB_METADATA_FINALIZERS_DERIVEDFINALIZER_HPP
