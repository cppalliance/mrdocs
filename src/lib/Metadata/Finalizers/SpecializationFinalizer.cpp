//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "SpecializationFinalizer.hpp"
#include <mrdocs/Support/Assert.hpp>
#include <algorithm>
#include <ranges>

namespace mrdocs {

namespace {

// Return the `SymbolID` of the primary class template that a
// deduction guide deduces, or `SymbolID::invalid` when it
// cannot be determined.
SymbolID
deducedPrimaryID(GuideSymbol const& guide)
{
    if (guide.Deduced.valueless_after_move()
        || !guide.Deduced->isNamed())
    {
        return SymbolID::invalid;
    }
    NamedType const& namedType = guide.Deduced->asNamed();
    if (namedType.Name.valueless_after_move())
    {
        return SymbolID::invalid;
    }
    return namedType.Name->id;
}

} // (unnamed)

void
SpecializationFinalizer::
processRecord(RecordSymbol const& I)
{
    if (!I.Template
        || I.Template->specializationKind() == TemplateSpecKind::Primary)
    {
        return;
    }
    Symbol* primary = corpus_.find(I.Template->Primary);
    if (!primary
        || primary->Extraction != ExtractionMode::Regular
        || !primary->isRecord())
    {
        return;
    }
    primary->asRecordPtr()->Specializations.push_back(I.id);
    Symbol* self = corpus_.find(I.id);
    MRDOCS_ASSERT(self && self->isRecord());
    self->asRecordPtr()->IsListedOnPrimary = true;
}

void
SpecializationFinalizer::
processFunction(FunctionSymbol const& I)
{
    if (!I.Template
        || I.Template->specializationKind() == TemplateSpecKind::Primary)
    {
        return;
    }
    Symbol* primary = corpus_.find(I.Template->Primary);
    if (!primary
        || primary->Extraction != ExtractionMode::Regular
        || !primary->isFunction())
    {
        return;
    }
    primary->asFunctionPtr()->Specializations.push_back(I.id);
    Symbol* self = corpus_.find(I.id);
    MRDOCS_ASSERT(self && self->isFunction());
    self->asFunctionPtr()->IsListedOnPrimary = true;
}

void
SpecializationFinalizer::
processGuide(GuideSymbol const& I)
{
    SymbolID const deducedId = deducedPrimaryID(I);
    if (!deducedId)
    {
        return;
    }
    Symbol* deduced = corpus_.find(deducedId);
    if (!deduced
        || deduced->Extraction != ExtractionMode::Regular
        || !deduced->isRecord())
    {
        return;
    }
    deduced->asRecordPtr()->DeductionGuides.push_back(I.id);
}

void
SpecializationFinalizer::
sortBackPointers()
{
    auto byReferentName = [this](SymbolID const& lhs, SymbolID const& rhs)
    {
        Symbol const* lhsInfo = corpus_.find(lhs);
        Symbol const* rhsInfo = corpus_.find(rhs);
        if (!lhsInfo || !rhsInfo)
        {
            return lhs < rhs;
        }
        if (lhsInfo->Name != rhsInfo->Name)
        {
            return lhsInfo->Name < rhsInfo->Name;
        }
        return lhs < rhs;
    };
    for (Symbol const& I : corpus_)
    {
        if (I.isRecord())
        {
            RecordSymbol const& R = I.asRecord();
            if (!R.Specializations.empty() || !R.DeductionGuides.empty())
            {
                RecordSymbol* mut = corpus_.find(I.id)->asRecordPtr();
                std::ranges::sort(mut->Specializations, byReferentName);
                std::ranges::sort(mut->DeductionGuides, byReferentName);
            }
        }
        else if (I.isFunction())
        {
            FunctionSymbol const& F = I.asFunction();
            if (!F.Specializations.empty())
            {
                FunctionSymbol* mut = corpus_.find(I.id)->asFunctionPtr();
                std::ranges::sort(mut->Specializations, byReferentName);
            }
        }
    }
}

void
SpecializationFinalizer::
build()
{
    for (Symbol const& I : corpus_)
    {
        if (I.Extraction == ExtractionMode::Regular)
        {
            if (I.isRecord())
            {
                processRecord(I.asRecord());
            }
            else if (I.isFunction())
            {
                processFunction(I.asFunction());
            }
            else if (I.isGuide())
            {
                processGuide(I.asGuide());
            }
        }
    }
    sortBackPointers();
}

} // mrdocs
