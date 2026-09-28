//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "HiddenFriendsFinalizer.hpp"
#include "BaseMembersFinalizer.hpp"
#include <mrdocs/Metadata/DocComment.hpp>
#include <mrdocs/Metadata/DocComment/Inline/ReferenceInline.hpp>
#include <mrdocs/Metadata/Name/IdentifierName.hpp>
#include <mrdocs/Metadata/Name/SpecializationName.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <mrdocs/Support/Report.hpp>
#include <algorithm>
#include <format>
#include <string_view>

namespace mrdocs {

namespace {

// A symbol has a page of its own only when it is Regular or SeeBelow. A
// Dependency or ImplementationDefined symbol is never rendered, so a
// reference to it would dangle.
bool
hasPage(ExtractionMode const M)
{
    return M == ExtractionMode::Regular ||
        M == ExtractionMode::SeeBelow;
}

bool
shouldCopy(
    ConfigSchema::BaseMemberInheritance const inherit,
    Symbol const& M)
{
    // A hidden friend is copied only when it has no page to link to: a
    // Dependency or ImplementationDefined symbol is never rendered. A SeeBelow
    // friend has a page, so it is referenced rather than duplicated.
    if (inherit == ConfigSchema::BaseMemberInheritance::CopyDependencies)
    {
        return !hasPage(M.Extraction);
    }
    return inherit == ConfigSchema::BaseMemberInheritance::CopyAll;
}

/** Visit every `NamedType` in a type tree.

    Recurses through reference, pointer, member-pointer, array, and function
    types and calls `f` with each `NamedType` found. `TypeT` is `Type` or
    `Type const`, so the callback receives a matching `NamedType&` or
    `NamedType const&`.
*/
template <class TypeT, class F>
void
forEachNamedType(TypeT& type, F const& f)
{
    switch (type.Kind)
    {
    case TypeKind::Named:
        f(type.asNamed());
        break;
    case TypeKind::LValueReference:
        forEachNamedType(*type.asLValueReference().PointeeType, f);
        break;
    case TypeKind::RValueReference:
        forEachNamedType(*type.asRValueReference().PointeeType, f);
        break;
    case TypeKind::Pointer:
        forEachNamedType(*type.asPointer().PointeeType, f);
        break;
    case TypeKind::MemberPointer:
        forEachNamedType(*type.asMemberPointer().ParentType, f);
        forEachNamedType(*type.asMemberPointer().PointeeType, f);
        break;
    case TypeKind::Array:
        forEachNamedType(*type.asArray().ElementType, f);
        break;
    case TypeKind::Function:
    {
        auto& fn = type.asFunction();
        forEachNamedType(*fn.ReturnType, f);
        for (auto& paramType : fn.ParamTypes)
        {
            forEachNamedType(*paramType, f);
        }
        break;
    }
    case TypeKind::Decltype:
    case TypeKind::Auto:
        break;
    }
}

/** Whether a type tree names the record `id`.

    True when any `NamedType` in the tree is the record itself or a
    specialization of it. A parameter of this type takes the record by value
    or by reference, so calling the function with an object of a derived
    class needs a derived-to-base conversion.
*/
inline
bool
namesRecord(Type const& type, SymbolID id)
{
    bool found = false;
    forEachNamedType(type, [&](NamedType const& named)
    {
        if (named.Name->id == id)
        {
            found = true;
        }
        else if (named.Name->isSpecialization() &&
                 named.Name->asSpecialization().specializationID == id)
        {
            found = true;
        }
    });
    return found;
}

/** Rewrite a type tree in place, replacing every `NamedType` that names the
    base `from` with one that names `to`.

    A bare identifier naming `from` is always substituted. A specialization of
    `from` (e.g. `crtp_base<derived>`) is substituted only when the primary
    template has no page of its own: a rendered specialization links to its
    primary template's page, so a documented base keeps its clickable operand.
    `hasPage` answers that question from the corpus and is provided by
    `BaseMembersFinalizer`.
*/
template <class HasPage>
void
replaceNamedType(
    Type& type,
    SymbolID from,
    SymbolID to,
    std::string_view toIdentifier,
    HasPage const& hasPage)
{
    forEachNamedType(type, [&](NamedType& named)
    {
        bool substitute = false;
        if (named.Name->isIdentifier())
        {
            substitute = named.Name->id == from;
        }
        else if (named.Name->isSpecialization())
        {
            auto const& spec = named.Name->asSpecialization();
            // `spec.id` is the primary template; `spec.specializationID`
            // names the concrete specialization when one exists.
            if ((named.Name->id == from ||
                 spec.specializationID == from) &&
                !hasPage(named.Name->id))
            {
                substitute = true;
            }
        }
        if (substitute)
        {
            IdentifierName replacement;
            replacement.id = to;
            replacement.Identifier.assign(toIdentifier);
            named.Name = Polymorphic<Name>(std::move(replacement));
        }
    });
}

} // (anon)

void
HiddenFriendsFinalizer::
inheritHiddenFriends(
    RecordSymbol const& I,
    RecordSymbol const& B,
    SymbolID const& fromId,
    AccessKind const A,
    SourceInfo const& baseLoc)
{
    // Whether a referenced id names a symbol with a page of its own. Used to
    // decide if a copied friend's operand must be collapsed to the derived
    // type: an operand naming an implementation-defined (or dependency) base
    // has no page to link to, so rendering it would produce a placeholder.
    auto const hasPageId = [&](SymbolID const id) -> bool
    {
        Symbol const* s = corpus_.find(id);
        return s && hasPage(s->Extraction);
    };

    // Handle one hidden friend. `substId` is the base id a copied
    // signature substitutes: the base as written in the base-specifier
    // for a friend the base itself declares, or the id the base already
    // substituted for a friend it inherited. `declaringId` is the class
    // that declares the friend, as written: the primary template for an
    // implicit specialization, which has no page and must not be named.
    // Every surfaced friend is recorded in inheritedHiddenFriends_ so a
    // class derived from this one surfaces it transitively.
    auto const process = [&](
        FunctionSymbol& friendFn,
        SymbolID const& substId,
        SymbolID const& declaringId)
    {
        // ADL finds a base's hidden friend through any base, but outside
        // code can only call it when no operand needs a derived-to-base
        // conversion: through a protected or private base that conversion is
        // inaccessible. A friend whose parameters never name the base (the
        // CRTP `operator==(Derived const&, Derived const&)` shape) stays
        // callable and is surfaced; one that takes the base itself is not.
        if (A != AccessKind::Public)
        {
            bool const needsConversion = std::ranges::any_of(
                friendFn.Params,
                [&](Param const& param)
                {
                    return namesRecord(*param.Type, substId) ||
                        namesRecord(*param.Type, fromId);
                });
            if (needsConversion)
            {
                return;
            }
        }

        // The record keeps an inherited entry even when it has no page of
        // its own (an intermediate implementation-defined or dependency
        // base), so a further derived class surfaces the friend
        // transitively; but a record without a page emits no copy and adds
        // no reference itself.
        bool const emit = hasPage(I.Extraction);
        bool copy = shouldCopy(config_.inheritHiddenFriends, friendFn);

        // When a copy is wanted and the derived class has a page, build it
        // first: its substituted signature decides whether the copy is kept
        // or the original is referenced instead.
        std::unique_ptr<Symbol> copySym;
        FunctionSymbol* copyFn = nullptr;
        NamespaceSymbol* ns = nullptr;
        if (copy && emit)
        {
            // Copy the hidden friend to the nearest visible ancestor
            // namespace of the derived class, so a friend declared in a
            // filtered `detail` namespace surfaces next to the derived class
            // that uses it rather than next to the base's (possibly
            // filtered) namespace.
            Symbol* nsPtr = corpus_.find(I.Parent);
            while (nsPtr &&
                   !(nsPtr->isNamespace() &&
                     nsPtr->Extraction == ExtractionMode::Regular))
            {
                nsPtr = corpus_.find(nsPtr->Parent);
            }
            MRDOCS_CHECK_OR(nsPtr && nsPtr->isNamespace());
            ns = nsPtr->asNamespacePtr();

            copySym = visit(friendFn, [&]<class T>(T const& other)
                -> std::unique_ptr<Symbol>
            {
                return std::make_unique<T>(other);
            });
            copySym->Parent = nsPtr->id;
            copySym->id = SymbolID::createFromString(
                std::format("{}-{}", toBase16Str(I.id), toBase16Str(friendFn.id)));
            copySym->InheritedFrom = declaringId;
            if (baseLoc.DefLoc || !baseLoc.Loc.empty())
            {
                copySym->Loc = baseLoc;
            }
            if (copySym->Extraction == ExtractionMode::Dependency ||
                copySym->Extraction == ExtractionMode::ImplementationDefined)
            {
                copySym->Extraction = I.Extraction;
            }

            copyFn = copySym->asFunctionPtr();
            MRDOCS_ASSERT(copyFn);
            auto const substituteCopy = [&](SymbolID const from)
            {
                // Only the parameter types are rewritten: operands name the
                // type a caller writes on the derived class, but the return
                // type is what the base's friend actually returns. A friend
                // returning `Base` by value returns `Base` after ADL finds
                // it, so rewriting the return type would claim a `Derived`
                // the call never yields.
                for (Param& param : copyFn->Params)
                {
                    replaceNamedType(*param.Type, from, I.id, I.Name, hasPageId);
                }
            };
            // Match the operand against the id the base substituted
            // (`substId`) and against the direct base as written (`fromId`).
            // A friend the base inherited names the ancestor the base
            // substituted, but a CRTP-style operand names the base itself
            // (the friend's template parameter instantiates to the
            // intermediate base), which only `fromId` matches.
            substituteCopy(substId);
            if (fromId != substId)
            {
                substituteCopy(fromId);
            }

            // A documented original whose signature the substitution left
            // unchanged already names the derived class: a CRTP friend
            // instantiated for it. A copy would be a second page for the
            // same function, so the original is referenced instead.
            if (hasPage(friendFn.Extraction) && sameSignature(friendFn, *copyFn))
            {
                copy = false;
            }
        }

        if (!copy)
        {
            // Reference the original from the derived class. A symbol
            // without a page of its own cannot be linked, so it is
            // skipped.
            if (!hasPage(friendFn.Extraction))
            {
                return;
            }
            inheritedHiddenFriends_[I.id].push_back(
                {friendFn.id, substId, SymbolID::invalid, declaringId});
            if (!emit)
            {
                return;
            }
            // An undocumented friend is still surfaced: give it a doc so the
            // `relates` entry below has somewhere to live, mirroring the copy
            // path.
            if (!friendFn.doc)
            {
                friendFn.doc.emplace();
            }
            auto& relates = friendFn.doc->relates;
            if (std::ranges::none_of(
                    relates,
                    [&](doc::ReferenceInline const& ref)
                    {
                        return ref.id == I.id;
                    }))
            {
                doc::ReferenceInline ref(I.Name);
                ref.id = I.id;
                relates.push_back(std::move(ref));
            }
            return;
        }

        if (!emit)
        {
            inheritedHiddenFriends_[I.id].push_back(
                {friendFn.id, substId, SymbolID::invalid, declaringId});
            return;
        }

        // Skip a copy whose substituted signature the derived class already
        // declares as a hidden friend of its own, or that a copy made from
        // another base already provides.
        bool alreadyDeclared = false;
        for (SymbolID const& ownId : hiddenFriends_[I.id])
        {
            Symbol* ownPtr = corpus_.find(ownId);
            if (!ownPtr || !ownPtr->isFunction())
            {
                continue;
            }
            if (sameSignature(ownPtr->asFunction(), *copyFn))
            {
                alreadyDeclared = true;
                break;
            }
        }
        if (!alreadyDeclared)
        {
            for (InheritedHiddenFriend const& made :
                 inheritedHiddenFriends_[I.id])
            {
                if (!made.copyId)
                {
                    continue;
                }
                Symbol* madePtr = corpus_.find(made.copyId);
                if (madePtr && madePtr->isFunction() &&
                    sameSignature(madePtr->asFunction(), *copyFn))
                {
                    alreadyDeclared = true;
                    break;
                }
            }
        }
        if (alreadyDeclared)
        {
            return;
        }

        if (!copyFn->doc)
        {
            copyFn->doc.emplace();
        }
        // The copy belongs to the derived class only; any `relates` entries
        // carried over from the base point back at the base's page.
        copyFn->doc->relates.clear();
        doc::ReferenceInline relatesRef(I.Name);
        relatesRef.id = I.id;
        copyFn->doc->relates.push_back(std::move(relatesRef));
        // The copy is the friend as seen from the derived class: like its
        // parameters and parent namespace, its befriending class is the
        // derived class, not the base whose page may not exist.
        copyFn->HiddenFriendOf = I.id;

        ns->Members.Functions.push_back(copySym->id);
        inheritedHiddenFriends_[I.id].push_back(
            {friendFn.id, substId, copySym->id, declaringId});
        corpus_.info_.insert(std::move(copySym));
    };

    // Friends the base itself declares, substituting its own base type.
    for (SymbolID const& friendId : hiddenFriends_[B.id])
    {
        Symbol* friendPtr = corpus_.find(friendId);
        MRDOCS_CHECK_OR_CONTINUE(friendPtr);
        if (!friendPtr->isFunction())
        {
            continue;
        }
        process(friendPtr->asFunction(), fromId, fromId);
    }

    // Friends the base inherited from its own bases, substituting the same
    // base id the base did.
    for (InheritedHiddenFriend const& made :
         inheritedHiddenFriends_[B.id])
    {
        Symbol* friendPtr = corpus_.find(made.friendId);
        MRDOCS_CHECK_OR_CONTINUE(friendPtr);
        if (!friendPtr->isFunction())
        {
            continue;
        }
        FunctionSymbol& friendFn = friendPtr->asFunction();
        if (!friendFn.HiddenFriendOf.has_value())
        {
            continue;
        }
        process(friendFn, made.fromId, made.declaringId);
    }
}

void
HiddenFriendsFinalizer::
build()
{
    indexHiddenFriends();
    // Every record is visited from a snapshot of the corpus, since
    // copies are inserted while inheriting; the snapshot is in source
    // order, so a derived class is handled after the ones declared
    // before it and the output stays deterministic.
    std::vector<SymbolID> const records = BaseMembersFinalizer::recordsInSourceOrder(corpus_);
    for (SymbolID const& id : records)
    {
        Symbol* symbol = corpus_.find(id);
        MRDOCS_CHECK_OR_CONTINUE(symbol && symbol->isRecord());
        operator()(symbol->asRecord());
    }
}

void
HiddenFriendsFinalizer::
indexHiddenFriends()
{
    for (auto const& symbol : corpus_.info_)
    {
        auto const* fn = symbol->asFunctionPtr();
        if (!fn || !fn->HiddenFriendOf)
        {
            continue;
        }
        hiddenFriends_[*fn->HiddenFriendOf].push_back(fn->id);
    }
    // The corpus is an unordered set, so each list is sorted to keep the
    // order of copies, and so the output, independent of iteration order.
    for (auto& [id, friends] : hiddenFriends_)
    {
        std::ranges::sort(friends);
    }
}

void
HiddenFriendsFinalizer::
operator()(RecordSymbol& I)
{
    report::trace(
        "Inheriting hidden friends for record '{}'",
        corpus_.Corpus::qualifiedName(I));
    MRDOCS_CHECK_OR(!finalized_.contains(I.id));
    // The base graph is walked for every record, not just regular ones: a
    // derived class can name a filtered-out (implementation-defined or
    // dependency) intermediate base, and hidden friends inherited through
    // it still belong on the derived page. A record without a page emits
    // nothing itself but records what a further derived class surfaces.
    for (BaseInfo const& baseI: I.Bases)
    {
        Optional<BaseMembersFinalizer::ResolvedBase> base =
            BaseMembersFinalizer::resolveBase(corpus_, config_, I, baseI);
        MRDOCS_CHECK_OR_CONTINUE(base);
        operator()(*base->Record);
        inheritHiddenFriends(
            I, *base->Record, base->AsWritten, baseI.Access, base->RelocateLoc);
    }
    finalized_.emplace(I.id);
}

} // mrdocs
