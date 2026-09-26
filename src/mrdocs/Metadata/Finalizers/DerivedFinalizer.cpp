//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "DerivedFinalizer.hpp"
#include "SymbolCompare.hpp"
#include <mrdocs/Support/Container/Algorithm.hpp>
#include <mrdocs/Support/Report.hpp>

namespace mrdocs {

void
DerivedFinalizer::
build()
{
    for (auto& I : corpus_.info_)
    {
        MRDOCS_ASSERT(I);
        MRDOCS_CHECK_OR_CONTINUE(I->Extraction == ExtractionMode::Regular);
        MRDOCS_CHECK_OR_CONTINUE(I->isRecord());
        auto& record = I->asRecord();
        MRDOCS_CHECK_OR_CONTINUE(!record.Bases.empty());
        for (BaseInfo& base: record.Bases)
        {
            MRDOCS_CHECK_OR_CONTINUE(base.Access == AccessKind::Public);
            MRDOCS_CHECK_OR_CONTINUE(base.Type);
            MRDOCS_ASSERT(!base.Type.valueless_after_move());
            MRDOCS_CHECK_OR_CONTINUE(base.Type->isNamed());
            auto& namedType = base.Type->asNamed();
            MRDOCS_CHECK_OR_CONTINUE(namedType.Name);
            SymbolID const namedSymbolID = namedType.Name->id;
            MRDOCS_CHECK_OR_CONTINUE(namedSymbolID != SymbolID::invalid);
            Symbol* baseInfoPtr = corpus_.find(namedSymbolID);
            MRDOCS_CHECK_OR_CONTINUE(baseInfoPtr);
            MRDOCS_CHECK_OR_CONTINUE(baseInfoPtr->isRecord());
            MRDOCS_CHECK_OR_CONTINUE(baseInfoPtr->Extraction == ExtractionMode::Regular);
            auto& baseRecord = baseInfoPtr->asRecord();
            MRDOCS_CHECK_OR_CONTINUE(!contains(baseRecord.Derived, record.id));
            insert_sorted(
                baseRecord.Derived,
                record.id,
                SymbolIDCompareFn{corpus_, config_});
        }
    }
}

} // mrdocs
