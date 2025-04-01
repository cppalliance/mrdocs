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
#include <mrdocs/Support/Report.hpp>
#include <mrdocs/Support/Algorithm.hpp>
#include <algorithm>

namespace clang::mrdocs {

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
            MRDOCS_CHECK_OR_CONTINUE(base.Type->isNamed());
            auto& namedType = dynamic_cast<NamedTypeInfo&>(*base.Type);
            MRDOCS_CHECK_OR_CONTINUE(namedType.Name);
            SymbolID const namedSymbolID = namedType.Name->id;
            MRDOCS_CHECK_OR_CONTINUE(namedSymbolID != SymbolID::invalid);
            Info* baseInfoPtr = corpus_.find(namedSymbolID);
            MRDOCS_CHECK_OR_CONTINUE(baseInfoPtr);
            MRDOCS_CHECK_OR_CONTINUE(baseInfoPtr->isRecord());
            MRDOCS_CHECK_OR_CONTINUE(baseInfoPtr->Extraction == ExtractionMode::Regular);
            if (auto& baseRecord = baseInfoPtr->asRecord();
                !contains(baseRecord.Derived, record.id))
            {
                // Insert in order by name
                auto const it = std::ranges::lower_bound(
                    baseRecord.Derived,
                    record.id,
                    [&](SymbolID const& lhs, SymbolID const& rhs) {
                        auto const* lhsRecord = corpus_.find(lhs);
                        auto const* rhsRecord = corpus_.find(rhs);
                        MRDOCS_ASSERT(lhsRecord);
                        MRDOCS_ASSERT(rhsRecord);
                        if (lhsRecord->Name != rhsRecord->Name)
                        {
                            return lhsRecord->Name < rhsRecord->Name;
                        }
                        return lhs < rhs;
                });
                baseRecord.Derived.insert(it, record.id);
            }
        }
    }
}

} // clang::mrdocs
