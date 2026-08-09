//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_METADATA_FINALIZERS_ANCHORFINALIZER_HPP
#define MRDOCS_LIB_METADATA_FINALIZERS_ANCHORFINALIZER_HPP

namespace mrdocs {

class Corpus;
class Config;

/** Populates the legible anchor name of every symbol.

    Owns the legible-name algorithm (see AnchorFinalizer.cpp): it computes
    a unique, filesystem- and URL-safe name for each symbol and stores it
    in `Symbol::Anchor`. Making it a reflected field lets any generator
    read it (as `anchor`) and build hrefs from it, instead of each
    generator rebuilding the name table.
*/
class AnchorFinalizer
{
    Corpus& corpus_;
    Config const& config_;

public:
    AnchorFinalizer(Corpus& corpus, Config const& config)
        : corpus_(corpus)
        , config_(config)
    {}

    /** Compute and store every symbol's `Anchor`. */
    void
    build();
};

} // mrdocs

#endif // MRDOCS_LIB_METADATA_FINALIZERS_ANCHORFINALIZER_HPP
