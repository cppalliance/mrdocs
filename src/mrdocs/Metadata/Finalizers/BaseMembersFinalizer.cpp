//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "BaseMembersFinalizer.hpp"
#include <mrdocs/Support/Container/Algorithm.hpp>
#include <mrdocs/Support/Report.hpp>
#include <format>
#include <tuple>

namespace mrdocs {

void
BaseMembersFinalizer::
inheritBaseMembers(
    RecordSymbol& I,
    RecordSymbol const& B,
    SymbolID const& baseId,
    AccessKind const A,
    SourceInfo const& baseLoc)
{
    inheritBaseMembers(I.id, baseId, I.Interface, B.Interface, A, baseLoc);
}

void
BaseMembersFinalizer::
inheritBaseMembers(
    SymbolID const& derivedId,
    SymbolID const& baseId,
    RecordInterface& derived,
    RecordInterface const& base,
    AccessKind const A,
    SourceInfo const& baseLoc)
{
    if (A == AccessKind::Public)
    {
        // When a class uses public member access specifier to derive from a
        // base, all public members of the base class are accessible as public
        // members of the derived class and all protected members of the base
        // class are accessible as protected members of the derived class.
        // Private members of the base are never accessible unless friended.
        inheritBaseMembers(derivedId, baseId, derived.Public, base.Public, baseLoc);
        inheritBaseMembers(derivedId, baseId, derived.Protected, base.Protected, baseLoc);
    }
    else if (A == AccessKind::Protected)
    {
        // When a class uses protected member access specifier to derive from a
        // base, all public and protected members of the base class are
        // accessible as protected members of the derived class (private members
        // of the base are never accessible unless friended).
        inheritBaseMembers(derivedId, baseId, derived.Protected, base.Public, baseLoc);
        inheritBaseMembers(derivedId, baseId, derived.Protected, base.Protected, baseLoc);
    }
    else if (A == AccessKind::Private && config_.extractPrivate)
    {
        // When a class uses private member access specifier to derive from a
        // base, all public and protected members of the base class are
        // accessible as private members of the derived class (private members
        // of the base are never accessible unless friended).
        inheritBaseMembers(derivedId, baseId, derived.Private, base.Public, baseLoc);
        inheritBaseMembers(derivedId, baseId, derived.Private, base.Protected, baseLoc);
    }
}

void
BaseMembersFinalizer::
inheritBaseMembers(
    SymbolID const& derivedId,
    SymbolID const& baseId,
    RecordTranche& derived,
    RecordTranche const& base,
    SourceInfo const& baseLoc)
{
    // Taken before anything is inherited, so that a member coming from a
    // base is never mistaken for one the derived class declares.
    std::unordered_set<std::string> const derivedNames = memberNames(derived);

    describe::for_each_member<RecordTranche>([&](auto const d) {
        inheritBaseMembers(
            derivedId, baseId, derived.*d.pointer, base.*d.pointer,
            derivedNames, baseLoc);
    });
}

std::unordered_set<std::string>
BaseMembersFinalizer::
memberNames(RecordTranche const& T) const
{
    std::unordered_set<std::string> result;
    describe::for_each_member<RecordTranche>([&](auto const d) {
        for (SymbolID const& id: T.*d.pointer)
        {
            if (Symbol const* infoPtr = corpus_.find(id))
            {
                result.insert(infoPtr->Name);
            }
        }
    });
    return result;
}

namespace {
bool
shouldCopy(Config const& config, Symbol const& M)
{
    if (config.inheritBaseMembers == ConfigSchema::BaseMemberInheritance::CopyDependencies)
    {
        return M.Extraction != ExtractionMode::Regular;
    }
    return config.inheritBaseMembers == ConfigSchema::BaseMemberInheritance::CopyAll;
}
}

void
BaseMembersFinalizer::
inheritBaseMembers(
    SymbolID const& derivedId,
    SymbolID const& baseId,
    std::vector<SymbolID>& derived,
    std::vector<SymbolID> const& base,
    std::unordered_set<std::string> const& derivedNames,
    SourceInfo const& baseLoc)
{
    Symbol const* derivedInfo = nullptr;
    auto const getDerivedInfo = [&]() -> Symbol const*
    {
        if (!derivedInfo)
        {
            derivedInfo = corpus_.find(derivedId);
        }
        return derivedInfo;
    };

    for (SymbolID const& otherID: base)
    {
        // Find the info from the base class
        MRDOCS_CHECK_OR_CONTINUE(!contains(derived, otherID));
        Symbol* otherInfoPtr = corpus_.find(otherID);
        MRDOCS_CHECK_OR_CONTINUE(otherInfoPtr);
        Symbol& otherInfo = *otherInfoPtr;

        // Check if we're not attempt to copy a special member function
        if (auto const *funcPtr = otherInfoPtr->asFunctionPtr()) {
          MRDOCS_CHECK_OR_CONTINUE(
              !is_one_of(funcPtr->FuncClass, {FunctionClass::Constructor,
                                              FunctionClass::Destructor}));
        }

        // A using-declaration re-exports a name rather than a signature,
        // so whatever the derived class declares under that name hides it,
        // whichever kind of member that is. The search below cannot see
        // it, since it covers the members of one kind.
        MRDOCS_CHECK_OR_CONTINUE(
            !otherInfo.isUsing() || !derivedNames.contains(otherInfo.Name));

        // Check if derived class has a member that shadows the base member
        auto shadowIt = std::ranges::find_if(
            derived,
            [&](SymbolID const& id)
            {
            Symbol* infoPtr = corpus_.find(id);
                MRDOCS_CHECK_OR(infoPtr, false);
                auto& info = *infoPtr;
                MRDOCS_CHECK_OR(info.Kind == otherInfo.Kind, false);
                if (info.isFunction())
                {
                    // If it's a function, it's only a shadow if the signatures
                    // are the same
                    auto const& otherFunc = static_cast<FunctionSymbol const&>(otherInfo);
                    auto const& func = static_cast<FunctionSymbol const&>(info);
                    return overrides(func, otherFunc);
                }
                // For other kinds of members, it's a shadow if the names
                // are the same
                return info.Name == otherInfo.Name;
            });
        MRDOCS_CHECK_OR_CONTINUE(shadowIt == derived.end());

        // Not a shadow, so inherit the base member
        if (!shouldCopy(config_, otherInfo))
        {
            // When it's a dependency, we don't create a reference to
            // the member because the reference would be invalid.
            // The user can use `copy-dependencies` or `copy` to
            // copy the dependencies.
            // There could be another option that forces the symbol
            // extraction mode to be regular, but that is controversial.
            if (otherInfo.Extraction != ExtractionMode::Dependency)
            {
                derived.push_back(otherID);
            }
        }
        else
        {
            std::unique_ptr<Symbol> otherCopy =
                visit(otherInfo, [&]<class T>(T const& other)
                    -> std::unique_ptr<Symbol>
                {
                    return std::make_unique<T>(other);
                });
            otherCopy->Parent = derivedId;
            otherCopy->id = SymbolID::createFromString(
                std::format("{}-{}", toBase16Str(otherCopy->Parent),
                            toBase16Str(otherInfo.id)));
            // A member the base itself inherited already names the class
            // that declares it; only a member the base declares is marked
            // as coming from the base.
            if (!otherCopy->InheritedFrom)
            {
                otherCopy->InheritedFrom = baseId;
            }
            // A base that is not itself a regular (documented) symbol - an
            // excluded or external base - has no page of its own, so its
            // members' locations point outside the documented project. For
            // those, `baseLoc` carries a replacement location (the
            // `: public Base` clause in the derived class, or the derived class
            // itself); a base that is a regular symbol - including a
            // specialization whose primary template is documented - leaves
            // `baseLoc` empty, so the member keeps its own real location.
            if (baseLoc.DefLoc || !baseLoc.Loc.empty())
            {
                otherCopy->Loc = baseLoc;
            }
            derived.push_back(otherCopy->id);
            // Get the extraction mode from the derived class
            if (otherCopy->Extraction == ExtractionMode::Dependency ||
                otherCopy->Extraction == ExtractionMode::ImplementationDefined)
            {
                Symbol const* derivedInfoPtr = getDerivedInfo();
                MRDOCS_CHECK_OR_CONTINUE(derivedInfoPtr);
                otherCopy->Extraction = derivedInfoPtr->Extraction;
            }
            corpus_.info_.insert(std::move(otherCopy));
        }
    }
}

std::vector<SymbolID>
BaseMembersFinalizer::
recordsInSourceOrder(Corpus const& corpus)
{
    std::vector<Symbol const*> records;
    for (auto const& symbol : corpus.info_)
    {
        if (symbol->isRecord())
        {
            records.push_back(symbol.get());
        }
    }
    // The definition when there is one, otherwise the first declaration.
    auto const location = [](Symbol const& s) -> Location const*
    {
        if (s.Loc.DefLoc)
        {
            return &*s.Loc.DefLoc;
        }
        if (!s.Loc.Loc.empty())
        {
            return &s.Loc.Loc.front();
        }
        return nullptr;
    };
    std::ranges::sort(records, [&](Symbol const* a, Symbol const* b)
    {
        Location const* la = location(*a);
        Location const* lb = location(*b);
        if (la && lb)
        {
            auto const key = [](Location const& l)
            {
                return std::tie(l.SourcePath, l.LineNumber, l.ColumnNumber);
            };
            if (key(*la) != key(*lb))
            {
                return key(*la) < key(*lb);
            }
        }
        else if (la || lb)
        {
            // Records without a location go last.
            return la != nullptr;
        }
        return a->id < b->id;
    });
    std::vector<SymbolID> ids;
    ids.reserve(records.size());
    for (Symbol const* record : records)
    {
        ids.push_back(record->id);
    }
    return ids;
}

void
BaseMembersFinalizer::
operator()(RecordSymbol& I)
{
    MRDOCS_CHECK_OR(I.Extraction == ExtractionMode::Regular);
    report::trace(
        "Extracting base members for record '{}'",
        corpus_.Corpus::qualifiedName(I));
    MRDOCS_CHECK_OR(!finalized_.contains(I.id));
    for (BaseInfo const& baseI: I.Bases)
    {
        MRDOCS_ASSERT(!baseI.Type.valueless_after_move());
        MRDOCS_CHECK_OR_CONTINUE(baseI.Type->isNamed());
        auto& baseNameType = baseI.Type->asNamed();
        MRDOCS_ASSERT(!baseNameType.Name.valueless_after_move());
        auto& baseName = baseNameType.Name->asName();
        // `baseName.id` is the primary template's ID. When the base
        // names a concrete specialization (e.g. `base<int>`) and
        // `extract-implicit-base-classes` is on, we prefer the
        // implicit specialization's ID so inherited members carry
        // the substituted types. For a dependent base such as
        // `base<T>` in `template<T> class derived : public base<T>`,
        // `specializationID` stays invalid (no `ClassTemplate-
        // SpecializationDecl` exists in the AST), so we fall back
        // to the primary's ID and inherit its members with the
        // primary's template parameter intact.
        SymbolID baseID = baseName.id;
        if (config_.extractImplicitBaseClasses && 
            baseName.isSpecialization())
        {
            auto& baseSpec = baseName.asSpecialization();
            if (baseSpec.specializationID)
            {
                baseID = baseSpec.specializationID;
            }
        }
        MRDOCS_CHECK_OR_CONTINUE(baseID);
        // A record can name a dependent specialization of its own template.
        // Explicit and partial specializations keep their own IDs.
        MRDOCS_CHECK_OR_CONTINUE(baseID != I.id);
        auto basePtr = corpus_.find(baseID);
        MRDOCS_CHECK_OR_CONTINUE(basePtr);
        auto* baseRecord = basePtr->asRecordPtr();
        MRDOCS_CHECK_OR_CONTINUE(baseRecord);
        operator()(*baseRecord);

        // Decide whether inherited members should be relocated. The base class
        // is documented on its own page only when the symbol it names is
        // regular; for a specialization that is the primary template
        // (`baseName.id`), so a specialization of a documented template counts
        // as documented. When it is documented, members keep their real
        // location; otherwise they are relocated to the base-specifier in the
        // derived class (falling back to the derived class's own location).
        Symbol const* namedBase = corpus_.find(baseName.id);
        SourceInfo relocateLoc;
        if (!namedBase || namedBase->Extraction != ExtractionMode::Regular)
        {
            relocateLoc = baseI.Loc.DefLoc || !baseI.Loc.Loc.empty()
                ? baseI.Loc
                : I.Loc;
        }
        // `baseName.id` is the base as written: for a specialization it is
        // the primary template, the class the reader can see, so it is what
        // copied members record as the class they are inherited from.
        inheritBaseMembers(I, *baseRecord, baseName.id, baseI.Access, relocateLoc);
    }
    finalized_.emplace(I.id);
}

} // mrdocs
