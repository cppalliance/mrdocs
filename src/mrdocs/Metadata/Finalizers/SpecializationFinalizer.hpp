//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_METADATA_FINALIZERS_SPECIALIZATIONFINALIZER_HPP
#define MRDOCS_LIB_METADATA_FINALIZERS_SPECIALIZATIONFINALIZER_HPP

#include <mrdocs/Corpus.hpp>

namespace mrdocs {

/** Populate the back-pointers from primaries to their specializations.

    For each class- or function-template specialization extracted
    in @ref ExtractionMode::Regular whose primary is also `Regular`,
    appends the specialization's ID to the primary's
    `Specializations` list and sets the specialization's
    `IsListedOnPrimary` flag. For each `Regular` deduction guide,
    appends its ID to the deduced record's `DeductionGuides`
    list. The populated lists are sorted by referent name then ID.

    Orphan specializations - those whose primary is not extracted
    in `Regular` mode - keep `IsListedOnPrimary` `false` so they
    remain reachable from the parent scope's listing.
*/
class SpecializationFinalizer
{
    Corpus& corpus_;
    Config const& config_;

    void processRecord(RecordSymbol const& I);
    void processFunction(FunctionSymbol const& I);
    void processGuide(GuideSymbol const& I);

    void sortBackPointers();

public:
    /** Construct the finalizer bound to a corpus.

        @param corpus The corpus whose specialization and deduction-guide
            back-pointers will be populated by `build`.
    */
    explicit
    SpecializationFinalizer(Corpus& corpus, Config const& config) noexcept
        : corpus_(corpus)
        , config_(config)
    {
    }

    /** Populate the back-pointers and sort them.

        Walks the corpus once, appending each `Regular` specialization
        to its primary's `Specializations` list (when the primary is
        also regular) and each `Regular` deduction guide to its deduced
        record's `DeductionGuides` list. Specializations whose primary
        will be rendered also get their `IsListedOnPrimary` flag set.
        Finally sorts every populated vector by referent name then ID.
    */
    void build();
};

} // mrdocs

#endif // MRDOCS_LIB_METADATA_FINALIZERS_SPECIALIZATIONFINALIZER_HPP
