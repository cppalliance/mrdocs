//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "lib/Lib/ConfigImpl.hpp"
#include "lib/Support/Debug.hpp"
#include "lib/Dom/LazyArray.hpp"
#include <mrdocs/Metadata/Interface.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Support/TypeTraits.hpp>
#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Metadata/Enum.hpp>
#include <mrdocs/Metadata/Function.hpp>
#include <mrdocs/Metadata/Overloads.hpp>
#include <mrdocs/Metadata/Record.hpp>
#include <mrdocs/Metadata/Typedef.hpp>
#include <mrdocs/Metadata/Variable.hpp>
#include <algorithm>
#include <ranges>
#include <variant>

namespace clang {
namespace mrdocs {

namespace {

class TrancheBuilder
{
    const Corpus& corpus_;
    const Info& parent_;
    Tranche* none_;
    Tranche* public_;
    Tranche* protected_;
    Tranche* private_;

    bool includePrivate_ = true;

    Tranche* trancheFor(AccessKind access)
    {
        switch(access)
        {
        case AccessKind::Public:
            return public_;
        case AccessKind::Protected:
            return protected_;
        case AccessKind::Private:
            return private_;
        case AccessKind::None:
            return none_;
        default:
            MRDOCS_UNREACHABLE();
        }
    }

    void
    push(
        ScopeInfo& S,
        const Info& I)
    {
        if(std::ranges::find(S.Members, I.id) == S.Members.end())
            S.Members.emplace_back(I.id);
        auto& lookups = S.Lookups.try_emplace(I.Name).first->second;
        if(std::ranges::find(lookups, I.id) == lookups.end())
            lookups.emplace_back(I.id);
    }

    void
    push(
        std::vector<SymbolID>& M,
        const Info& I)
    {
        if(std::ranges::find(M, I.id) == M.end())
            M.emplace_back(I.id);
    }

    template<typename ListTy>
    void
    push(
        ListTy Tranche::* list,
        AccessKind access,
        const Info& member)
    {
        if(Tranche* tranche = trancheFor(access))
            push(tranche->*list, member);
    }

    static
    AccessKind
    effectiveAccess(
        AccessKind memberAccess,
        AccessKind baseAccess) noexcept
    {
        if(memberAccess == AccessKind::None ||
            baseAccess == AccessKind::None)
            return AccessKind::None;
        if(memberAccess == AccessKind::Private ||
            baseAccess == AccessKind::Private)
            return AccessKind::Private;
        if(memberAccess == AccessKind::Protected ||
            baseAccess == AccessKind::Protected)
            return AccessKind::Protected;
        return AccessKind::Public;
    }

    bool
    isFromParent(const Info& I)
    {
        return
            I.Parent &&
            I.Parent == parent_.id;
    }

    const Info*
    lookThroughTypedefs(const Info* I)
    {
        if(! I || ! I->isTypedef())
            return I;
        auto* TI = static_cast<const TypedefInfo*>(I);
        return lookThroughTypedefs(
            corpus_.find(TI->Type->namedSymbol()));
    }

public:
    TrancheBuilder(
        const Corpus& corpus,
        const Info& Parent,
        Tranche* None,
        Tranche* Public,
        Tranche* Protected,
        Tranche* Private)
        : corpus_(corpus)
        , parent_(Parent)
        , none_(None)
        , public_(Public)
        , protected_(Protected)
        , private_(Private)
    {
        auto& config = static_cast<
            ConfigImpl const&>(corpus_.config);
        includePrivate_ = config->inaccessibleMembers !=
            ConfigImpl::SettingsImpl::ExtractPolicy::Never;
    }

    void
    add(
        const SymbolID& id,
        AccessKind baseAccess)
    {
        const auto& I = corpus_.get<Info>(id);
        auto actualAccess = effectiveAccess(I.Access, baseAccess);
        visit(I, *this, actualAccess);
    }

    void
    addFrom(
        const RecordInfo& I,
        AccessKind baseAccess)
    {
        for(auto const& B : I.Bases)
        {
            auto actualAccess = effectiveAccess(baseAccess, B.Access);

            if( ! includePrivate_ &&
                actualAccess == AccessKind::Private)
                continue;
            const Info* Base = lookThroughTypedefs(
                corpus_.find(B.Type->namedSymbol()));
            if(! Base || Base->id == I.id ||
                ! Base->isRecord())
                continue;
            addFrom(*static_cast<
                const RecordInfo*>(Base), actualAccess);
        }
        for(auto const& id : I.Members)
            add(id, baseAccess);
    }

    void addFrom(const NamespaceInfo& I)
    {
        for(auto const& id : I.Members)
            add(id, AccessKind::None);
    }

    void operator()(
        const NamespaceInfo& I,
        AccessKind access)
    {
        push(&Tranche::Namespaces, access, I);
    }

    void operator()(
        const RecordInfo& I,
        AccessKind access)
    {
        push(&Tranche::Records, access, I);
    }

    void operator()(
        const SpecializationInfo& I,
        AccessKind access)
    {
        // KRYSTIAN FIXME: currently unimplemented
    }

    void operator()(
        const FunctionInfo& I,
        AccessKind access)
    {
        // do not inherit constructors or destructors
        if(parent_.isRecord() && ! isFromParent(I) &&
            (I.Class == FunctionClass::Constructor ||
            I.Class == FunctionClass::Destructor))
            return;

        bool isStatic = I.StorageClass == StorageClassKind::Static;
        if(! parent_.isRecord() || ! isStatic)
        {
            push(&Tranche::Functions, access, I);
            push(&Tranche::Overloads, access, I);
        }
        else if(isStatic)
        {
            push(&Tranche::StaticFunctions, access, I);
            push(&Tranche::StaticOverloads, access, I);
        }
    }

    void operator()(
        const TypedefInfo& I,
        AccessKind access)
    {
        push(&Tranche::Types, access, I);
    }

    void operator()(
        const EnumInfo& I,
        AccessKind access)
    {
        push(&Tranche::Enums, access, I);
    }

    void operator()(
        const FieldInfo& I,
        AccessKind access)
    {
        push(&Tranche::Fields, access, I);
    }

    void operator()(
        const VariableInfo& I,
        AccessKind access)
    {
        push(&Tranche::Variables, access, I);
    }

    void operator()(
        const FriendInfo& I,
        AccessKind access)
    {
        push(&Tranche::Friends, access, I);
    }

    void operator()(
        NamespaceAliasInfo const& I,
        AccessKind access)
    {
        push(&Tranche::NamespaceAliases, access, I);
    }

    void operator()(
        UsingInfo const& I,
        AccessKind access)
    {
        push(&Tranche::Usings, access, I);
    }

    void operator()(
        const EnumConstantInfo& I,
        AccessKind access)
    {
        // KRYSTIAN FIXME: currently unimplemented
    }

    void operator()(
        const GuideInfo& I,
        AccessKind access)
    {
        // declarations of deduction guides are not inherited
        if(parent_.isRecord() && ! isFromParent(I))
            return;
        push(&Tranche::Guides, access, I);
    }

    void operator()(
        ConceptInfo const& I,
        AccessKind access)
    {
        push(&Tranche::Concepts, access, I);
    }
};

void
buildTranches(
    const Corpus& corpus,
    const Info& I,
    Tranche* None,
    Tranche* Public,
    Tranche* Protected,
    Tranche* Private)
{
    TrancheBuilder builder(
        corpus,
        I,
        None,
        Public,
        Protected,
        Private);
    visit(I, [&]<typename InfoTy>(const InfoTy& II)
    {
        if constexpr(InfoTy::isRecord())
            builder.addFrom(II, AccessKind::Public);
        if constexpr(InfoTy::isNamespace())
            builder.addFrom(II);
    });
}

} // (anon)

Interface::
Interface(
    Corpus const& corpus_) noexcept
    : corpus(corpus_)
{
    Public = std::make_shared<Tranche>();
    Protected = std::make_shared<Tranche>();
    Private = std::make_shared<Tranche>();
}

Interface
makeInterface(
    RecordInfo const& Derived,
    Corpus const& corpus)
{
    Interface I(corpus);
    buildTranches(corpus, Derived, nullptr,
        I.Public.get(), I.Protected.get(), I.Private.get());
    return I;
}

Tranche
makeTranche(
    NamespaceInfo const& Namespace,
    Corpus const& corpus)
{
    Tranche T;
    buildTranches(corpus, Namespace, &T,
        nullptr, nullptr, nullptr);
    return T;
}

/*  A DOM object that represents a tranche

    This function creates an Interface object for a given
    record. The Interface object is used to generate the
    "interface" value of the DOM for symbols that represent
    records or namespaces.

    The interface is not part of the Corpus. It is a temporary
    structure generated to aggregate the symbols of a record.
    This structure is provided to the user via the DOM.
 */
class DomTranche : public dom::DefaultObjectImpl
{
    std::shared_ptr<Tranche> tranche_;

    static
    dom::Value
    init(
        std::span<const SymbolID> list,
        DomCorpus const& domCorpus)
    {
        return dom::LazyArray(list, [&](SymbolID const& id)
            {
                return domCorpus.get(id);
            });
    }

    static
    dom::Value
    init(
        const ScopeInfo& scope,
        DomCorpus const& domCorpus)
    {
        return generateScopeOverloadsArray(scope, domCorpus);
    }


public:
    DomTranche(
        std::shared_ptr<Tranche> const& tranche,
        DomCorpus const& domCorpus) noexcept
        : dom::DefaultObjectImpl({
            #define INFO(Plural, LC_Plural) \
            { #LC_Plural, init(tranche->Plural, domCorpus) },
            #include <mrdocs/Metadata/InfoNodesPascalPluralAndLowerPlural.inc>
            { "types",            init(tranche->Types, domCorpus) },
            { "staticfuncs",      init(tranche->StaticFunctions, domCorpus) },
            { "overloads",        init(tranche->Overloads, domCorpus) },
            { "staticoverloads",  init(tranche->StaticOverloads, domCorpus) },
            })
        , tranche_(tranche)
    {
    }
};

void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    std::shared_ptr<Tranche> const& sp,
    DomCorpus const* domCorpus)
{
    /* Unfortunately, we cannot use LazyObject like we do
       in DomCorpus because the Tranche object is not
       part of the Corpus.

       We must create a new object and eagerly populate it
       with the values from the Tranche object.

       Unfortunately, we cannot use the default
       dom::Object type either.

       We also need a custom object impl type because we
       need to store a shared_ptr to the Tranche object
       to keep it alive.
     */
    if (!sp)
    {
        v = nullptr;
        return;
    }
    v = dom::newObject<DomTranche>(sp, *domCorpus);
}

void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    std::shared_ptr<Interface> const& sp,
    DomCorpus const* domCorpus)
{
    /* Unfortunately, we cannot use LazyObject like we do
       in DomCorpus because the Interface object is not
       part of the Corpus.

       We must create a new object and eagerly populate it
       with the values from the individual Tranche objects.
     */
    if (!sp)
    {
        v = nullptr;
        return;
    }
    // The dom::Value for each tranche will keep the
    // respective shared_ptr alive.
    v = dom::Object({
        { "public", dom::ValueFrom(sp->Public, domCorpus) },
        { "protected", dom::ValueFrom(sp->Protected, domCorpus) },
        { "private", dom::ValueFrom(sp->Private, domCorpus) }
    });
}


} // mrdocs
} // clang
