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
#include <mrdocs/Metadata/Interface.hpp>
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
        return ! I.Namespace.empty() &&
            I.Namespace.front() == parent_.id;
    }

    const Info*
    getTypeAsTag(
        const std::unique_ptr<TypeInfo>& T)
    {
        if(! T)
            return nullptr;
        SymbolID id = visit(*T, []<typename T>(const T& t)
        {
            if constexpr(T::isTag() || T::isSpecialization())
                return t.id;
            return SymbolID::invalid;
        });
        if(! id)
            return nullptr;
        return corpus_.find(id);
    }


    const Info*
    lookThroughTypedefs(const Info* I)
    {
        if(! I || ! I->isTypedef())
            return I;
        auto* TI = static_cast<const TypedefInfo*>(I);
        return lookThroughTypedefs(getTypeAsTag(TI->Type));
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
                getTypeAsTag(B.Type));
            if(! Base || ! Base->isRecord())
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
        // do not inherit constructors, destructors,
        // or deduction guides
        if(parent_.isRecord() && ! isFromParent(I) &&
            (I.Class == FunctionClass::Constructor ||
            I.Class == FunctionClass::Destructor ||
            I.Class == FunctionClass::Deduction))
            return;

        bool isStatic = I.specs0.storageClass == StorageClassKind::Static;
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
        const EnumeratorInfo& I,
        AccessKind access)
    {
        // KRYSTIAN FIXME: currently unimplemented
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


} // mrdocs
} // clang
