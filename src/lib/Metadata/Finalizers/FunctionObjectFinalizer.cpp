//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "FunctionObjectFinalizer.hpp"
#include <mrdocs/Metadata/DocComment.hpp>
#include <mrdocs/Metadata/Specifiers/OperatorKind.hpp>
#include <mrdocs/Metadata/Symbol/Function.hpp>
#include <mrdocs/Metadata/Symbol/FunctionClass.hpp>
#include <mrdocs/Metadata/Symbol/Namespace.hpp>
#include <mrdocs/Metadata/Symbol/Param.hpp>
#include <mrdocs/Metadata/Symbol/Record.hpp>
#include <mrdocs/Metadata/Symbol/RecordTranche.hpp>
#include <mrdocs/Metadata/Symbol/SymbolBase.hpp>
#include <mrdocs/Metadata/Symbol/Variable.hpp>
#include <mrdocs/Support/Assert.hpp>
#include <mrdocs/Support/Report.hpp>
#include <algorithm>
#include <format>

namespace mrdocs {

namespace {

// Reset the CXXMethodDecl-specific fields that do not apply
// to a free function synthesized from an operator() overload.
void
clearMethodQualifiers(FunctionSymbol& F)
{
    F.FuncClass                     = FunctionClass::Normal;
    F.OverloadedOperator            = OperatorKind::None;
    F.IsRecordMethod                = false;
    F.IsConst                       = false;
    F.IsVolatile                    = false;
    F.IsVirtual                     = false;
    F.IsVirtualAsWritten            = false;
    F.IsPure                        = false;
    F.IsFinal                       = false;
    F.RefQualifier                  = ReferenceKind::None;
    F.HasOverrideAttr               = false;
    F.IsExplicitObjectMemberFunction = false;
    F.Explicit                      = {};
}

} // anonymous namespace

bool
FunctionObjectFinalizer::
isEnclosingScope(
    SymbolID ancestor,
    SymbolID descendant) const
{
    // Walk from descendant upward through the parent chain.
    // Return true if ancestor appears anywhere in that chain.
    SymbolID cur = descendant;
    while (cur)
    {
        if (cur == ancestor)
        {
            return true;
        }
        Symbol const* sym = corpus_.find(cur);
        if (!sym)
        {
            break;
        }
        cur = sym->Parent;
    }
    return false;
}

bool
FunctionObjectFinalizer::
isFunctionObjectType(
    RecordSymbol const& R,
    bool allowTemplates) const
{
    // A function object type is a record whose only public
    // non-special members are operator() overloads.
    // Class templates are excluded unless allowTemplates is
    // true, which prevents false positives from types like
    // std::hash<T> that match the heuristic but are not
    // function objects. The caller passes allowTemplates=true
    // when the type lives in a scope enclosed by the
    // variable's parent (e.g. a ::detail sub-namespace).
    if (R.Template && !allowTemplates)
    {
        return false;
    }

    // Iterate every public member. Skip special member functions
    // (C++ standard, [special]) and accept operator() overloads;
    // reject anything else.
    bool hasCallOperator = false;
    for (SymbolID const& mid : allMembers(R.Interface.Public))
    {
        Symbol const* sym = corpus_.find(mid);
        MRDOCS_CHECK_OR(sym, false);

        auto const* func = sym->asFunctionPtr();
        if (!func)
        {
            // Non-function member.
            return false;
        }

        if (!isSpecialMemberFunction(*func))
        {
            if (func->OverloadedOperator != OperatorKind::Call)
            {
                return false;
            }
            hasCallOperator = true;
        }
    }
    return hasCallOperator;
}

std::vector<SymbolID>
FunctionObjectFinalizer::
findCallOperatorOverloads(RecordSymbol const& R) const
{
    std::vector<SymbolID> result;
    for (SymbolID const& fid : R.Interface.Public.Functions)
    {
        Symbol const* sym = corpus_.find(fid);
        MRDOCS_CHECK_OR_CONTINUE(sym);
        auto const* func = sym->asFunctionPtr();
        MRDOCS_CHECK_OR_CONTINUE(func);
        if (func->OverloadedOperator == OperatorKind::Call)
        {
            result.push_back(fid);
        }
    }
    return result;
}

void
FunctionObjectFinalizer::
markImplementationDefined(RecordSymbol& R)
{
    R.Extraction = ExtractionMode::ImplementationDefined;
    for (SymbolID const& childId : allMembers(R.Interface))
    {
        Symbol* child = corpus_.find(childId);
        if (child)
        {
            child->Extraction = ExtractionMode::ImplementationDefined;
            if (auto* nested = child->asRecordPtr())
            {
                markImplementationDefined(*nested);
            }
        }
    }
}

bool
FunctionObjectFinalizer::
processVariable(
    VariableSymbol& V,
    std::vector<SymbolID>& sourceList,
    std::vector<SymbolID>& targetList)
{
    // Resolve the variable's type to a record.
    if (V.Type.valueless_after_move())
    {
        return false;
    }
    if (!(*V.Type).isNamed())
    {
        return false;
    }

    SymbolID const recordId = (*V.Type).namedSymbol();
    if (!recordId)
    {
        return false;
    }

    Symbol* recordSym = corpus_.find(recordId);
    if (!recordSym)
    {
        return false;
    }

    auto* record = recordSym->asRecordPtr();
    if (!record)
    {
        return false;
    }

    // Check if it qualifies: auto-detection or explicit
    // @functionobject doc command.
    //
    // Allow class templates when the function object type lives in
    // a scope enclosed by (or equal to) the variable's parent. This
    // catches patterns like detail::hash_fn<T> with hash in the outer
    // namespace, while excluding unrelated types like std::hash<T>.
    // Same-parent is always valid (even at global scope). The ancestor
    // walk is only used when V.Parent is not the global namespace,
    // because global is the root of every parent chain and would
    // match everything.
    bool const sameParent = record->Parent == V.Parent;
    bool const ancestorScope =
        V.Parent != SymbolID::global &&
        isEnclosingScope(V.Parent, record->Parent);
    bool const enclosed = sameParent || ancestorScope;
    bool const autoDetect =
        corpus_.config->autoFunctionObjects &&
        isFunctionObjectType(*record, /*allowTemplates=*/enclosed);
    bool const docMarked =
        (V.doc && V.doc->IsFunctionObject) ||
        (record->doc && record->doc->IsFunctionObject);
    if (!autoDetect && !docMarked)
    {
        return false;
    }

    // Find operator() overloads.
    std::vector<SymbolID> callOverloads = findCallOperatorOverloads(*record);
    if (callOverloads.empty())
    {
        if (docMarked)
        {
            auto loc = getPrimaryLocation(V);
            report::warn(
                "{}:{}:{}: '{}': @functionobject/@functor command used "
                "but type has no operator() overloads",
                loc ? loc->SourcePath : "<unknown>",
                loc ? loc->LineNumber : 0,
                loc ? loc->ColumnNumber : 0,
                V.Name);
        }
        return false;
    }

    // For each operator() overload, create a synthetic
    // FunctionSymbol named after the variable. The OverloadsFinalizer
    // (which runs later) will naturally group same-named functions
    // into an OverloadsSymbol when there are multiple overloads.
    markImplementationDefined(*record);
    for (SymbolID const& oid : callOverloads)
    {
        Symbol const* origSym = corpus_.find(oid);
        MRDOCS_CHECK_OR_CONTINUE(origSym);
        auto const* original = origSym->asFunctionPtr();
        MRDOCS_CHECK_OR_CONTINUE(original);

        // Copy the entire operator() signature, then override
        // identity and strip method-specific qualifiers.
        auto synth = std::make_unique<FunctionSymbol>(*original);
        synth->id = SymbolID::createFromString(
            std::format("{}-funobj-{}",
                toBase16Str(V.id), toBase16Str(oid)));
        synth->Name       = V.Name;
        synth->Parent     = V.Parent;
        synth->Access     = V.Access;
        synth->Extraction = ExtractionMode::Regular;
        clearMethodQualifiers(*synth);

        // Template: use the variable's template info when the
        // variable is a variable template.
        if (V.Template)
        {
            synth->Template = V.Template;
        }

        // Back-reference to the implementation type.
        synth->FunctionObjectImpl = recordId;

        // Documentation: prefer the operator()'s doc (which has
        // param/return details); fall back to the variable's doc.
        if (!synth->doc && V.doc)
        {
            synth->doc = V.doc;
        }

        targetList.push_back(synth->id);
        corpus_.info_.emplace(std::move(synth));
    }

    // Remove the variable from its parent's member list and
    // mark it implementation-defined.
    V.Extraction = ExtractionMode::ImplementationDefined;
    auto it = std::ranges::find(sourceList, V.id);
    if (it != sourceList.end())
    {
        sourceList.erase(it);
    }

    return true;
}

void
FunctionObjectFinalizer::
processNamespace(NamespaceSymbol& NS)
{
    // Process variables in this namespace.
    // We iterate over a copy of the variable IDs because processVariable()
    // may erase from the original vector.
    auto varIds = NS.Members.Variables;
    for (SymbolID const& vid : varIds)
    {
        Symbol* sym = corpus_.find(vid);
        MRDOCS_CHECK_OR_CONTINUE(sym);
        auto* var = sym->asVariablePtr();
        MRDOCS_CHECK_OR_CONTINUE(var);
        processVariable(*var, NS.Members.Variables, NS.Members.Functions);
    }

    // Recurse into sub-namespaces.
    for (SymbolID const& nsId : NS.Members.Namespaces)
    {
        Symbol* sym = corpus_.find(nsId);
        MRDOCS_CHECK_OR_CONTINUE(sym);
        auto* childNS = sym->asNamespacePtr();
        MRDOCS_CHECK_OR_CONTINUE(childNS);
        processNamespace(*childNS);
    }

    // Recurse into records.
    for (SymbolID const& recId : NS.Members.Records)
    {
        Symbol* sym = corpus_.find(recId);
        MRDOCS_CHECK_OR_CONTINUE(sym);
        auto* childRec = sym->asRecordPtr();
        MRDOCS_CHECK_OR_CONTINUE(childRec);
        processRecord(*childRec);
    }
}

void
FunctionObjectFinalizer::
processRecord(RecordSymbol& R)
{
    // Process static variables in each access tranche.
    RecordTranche* tranches[] = {
        &R.Interface.Public,
        &R.Interface.Protected,
        &R.Interface.Private,
    };

    for (RecordTranche* tranche : tranches)
    {
        auto staticVarIds = tranche->StaticVariables;
        for (SymbolID const& vid : staticVarIds)
        {
            Symbol* sym = corpus_.find(vid);
            MRDOCS_CHECK_OR_CONTINUE(sym);
            auto* var = sym->asVariablePtr();
            MRDOCS_CHECK_OR_CONTINUE(var);
            processVariable(
                *var,
                tranche->StaticVariables,
                tranche->StaticFunctions);
        }
    }

    // Recurse into nested records.
    for (RecordTranche* tranche : tranches)
    {
        for (SymbolID const& recId : tranche->Records)
        {
            Symbol* sym = corpus_.find(recId);
            MRDOCS_CHECK_OR_CONTINUE(sym);
            auto* childRec = sym->asRecordPtr();
            MRDOCS_CHECK_OR_CONTINUE(childRec);
            processRecord(*childRec);
        }
    }
}

void
FunctionObjectFinalizer::
build()
{
    auto* globalPtr = corpus_.find(SymbolID::global);
    MRDOCS_CHECK_OR(globalPtr);
    MRDOCS_ASSERT(globalPtr->isNamespace());
    processNamespace(globalPtr->asNamespace());
}

} // mrdocs
