//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "lib/Support/Radix.hpp"
#include "lib/Support/LegibleNames.hpp"
#include "lib/Dom/LazyObject.hpp"
#include "lib/Dom/LazyArray.hpp"
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <memory>
#include <mutex>
#include <variant>

namespace clang {
namespace mrdocs {

namespace {

//------------------------------------------------
//
// Helpers
//
//------------------------------------------------

/* Get overloads for a scope

    This function takes a type derived from ScopeInfo,
    such as Namespace or Record, and returns a vector
    of overloads in this scope using the DomCorpus
    to resolve the SymbolIDs in this scope.

    If the symbol in the scope has an ID, the ID
    is included in the result.
    Otherwise, the symbol is an OverloadSet, and
    the OverloadSet is included.

    Unfortunately, this information is not readily
    available in the Corpus, so we can't have lazy
    references to them.
    Instead, we need to traverse the overloads and
    generate the data whenever the information
    is requested.
 */
template <std::derived_from<ScopeInfo> ScopeInfoTy>
dom::Array
generateScopeOverloadsArray(
    ScopeInfoTy const& I,
    DomCorpus const& domCorpus)
{
    dom::Array res;
    domCorpus->traverseOverloads(I,
        [&](const auto& C)
    {
        if constexpr(requires { C.id; })
        {
            res.push_back(domCorpus.get(C.id));
        }
        else
        {
            res.push_back(domCorpus.getOverloads(C));
        }
    });
    return res;
}

//------------------------------------------------
//
// domCreate
//
//------------------------------------------------

static dom::Value domCreate(
    std::unique_ptr<TypeInfo> const&, DomCorpus const&);
static dom::Value domCreate(
    std::unique_ptr<TArg> const&, DomCorpus const&);
static dom::Value domCreate(
    std::unique_ptr<TParam> const&, DomCorpus const&);
static dom::Value domCreate(
    std::unique_ptr<TemplateInfo> const& I, DomCorpus const&);
static dom::Value domCreate(
    std::unique_ptr<NameInfo> const& I, DomCorpus const&);

static
dom::Value
domCreate(
    std::unique_ptr<TArg> const& I,
    DomCorpus const& domCorpus)
{
    if(! I)
        return nullptr;
    dom::Object::storage_type entries = {
        { "kind", toString(I->Kind) },
        { "is-pack", I->IsPackExpansion }
    };
    visit(*I, [&]<typename T>(const T& t)
        {
            if constexpr(T::isType())
            {
                entries.emplace_back("type",
                    domCreate(t.Type, domCorpus));
            }
            if constexpr(T::isNonType())
            {
                entries.emplace_back("value",
                    t.Value.Written);
            }
            if constexpr(T::isTemplate())
            {
                entries.emplace_back("name",
                    t.Name);
                entries.emplace_back("template",
                    domCorpus.get(t.Template));
            }
        });
    return dom::Object(std::move(entries));
}

static
dom::Value
domCreate(
    std::unique_ptr<TParam> const& I,
    DomCorpus const& domCorpus)
{
    if(! I)
        return nullptr;
    dom::Object::storage_type entries = {
        { "kind", toString(I->Kind) },
        { "name", dom::stringOrNull(I->Name) },
        { "is-pack", I->IsParameterPack }
    };
    visit(*I, [&]<typename T>(const T& t)
        {
            if(t.Default)
                entries.emplace_back("default",
                    domCreate(t.Default, domCorpus));

            if constexpr(T::isType())
            {
                entries.emplace_back("key",
                    toString(t.KeyKind));
                if(t.Constraint)
                    entries.emplace_back("constraint",
                        domCreate(t.Constraint, domCorpus));
            }
            if constexpr(T::isNonType())
            {
                entries.emplace_back("type",
                    domCreate(t.Type, domCorpus));
            }
            if constexpr(T::isTemplate())
            {
                entries.emplace_back("params",
                    dom::LazyArray(t.Params, [&](std::unique_ptr<TParam> const& I)
                    {
                        return domCreate(I, domCorpus);
                    }));
            }
        });
    return dom::Object(std::move(entries));
}

static
dom::Value
domCreate(
    std::unique_ptr<TemplateInfo> const& I,
    DomCorpus const& domCorpus)
{
    if(! I)
        return nullptr;
    return dom::Object({
        { "kind", toString(I->specializationKind()) },
        { "primary", domCorpus.get(I->Primary) },
        { "params", dom::LazyArray(I->Params, [&](std::unique_ptr<TParam> const& I)
            {
                return domCreate(I, domCorpus);
            }) },
        { "args", dom::LazyArray(I->Args, [&](std::unique_ptr<TArg> const& I)
            {
                return domCreate(I, domCorpus);
            }) },
        { "requires", dom::stringOrNull(I->Requires.Written) }
        });
}

//------------------------------------------------

static
dom::Value
domCreate(
    std::unique_ptr<NameInfo> const& I,
    DomCorpus const& domCorpus)
{
    if(! I)
        return nullptr;
    dom::Object::storage_type entries = {
        { "kind", toString(I->Kind) }
    };
    visit(*I, [&]<typename T>(const T& t)
    {
        entries.emplace_back("name", t.Name);
        entries.emplace_back("symbol", domCorpus.get(t.id));

        if constexpr(requires { t.TemplateArgs; })
            entries.emplace_back("args",
                dom::LazyArray(t.TemplateArgs, [&](std::unique_ptr<TArg> const& I)
                {
                    return domCreate(I, domCorpus);
                }));
        entries.emplace_back("prefix", domCreate(t.Prefix, domCorpus));
    });
    return dom::Object(std::move(entries));
}

static
dom::Value
domCreate(
    std::unique_ptr<TypeInfo> const& I,
    DomCorpus const& domCorpus)
{
    if(! I)
        return nullptr;
    dom::Object::storage_type entries = {
        { "kind", toString(I->Kind) },
        { "is-pack", I->IsPackExpansion },
    };
    visit(*I, [&]<typename T>(const T& t)
    {
        if constexpr(T::isNamed())
            entries.emplace_back("name",
                domCreate(t.Name, domCorpus));

        if constexpr(T::isDecltype())
            entries.emplace_back("operand", t.Operand.Written);

        if constexpr(T::isAuto())
        {
            entries.emplace_back("keyword",
                toString(t.Keyword));
            if(t.Constraint)
                entries.emplace_back("constraint",
                    domCreate(t.Constraint, domCorpus));
        }

        if constexpr(requires { t.CVQualifiers; })
            entries.emplace_back("cv-qualifiers",
                toString(t.CVQualifiers));

        if constexpr(requires { t.ParentType; })
            entries.emplace_back("parent-type",
                domCreate(t.ParentType, domCorpus));

        if constexpr(requires { t.PointeeType; })
            entries.emplace_back("pointee-type",
                domCreate(t.PointeeType, domCorpus));

        if constexpr(T::isArray())
        {
            entries.emplace_back("element-type",
                domCreate(t.ElementType, domCorpus));
            if(t.Bounds.Value)
                entries.emplace_back("bounds-value",
                    *t.Bounds.Value);
            entries.emplace_back("bounds-expr",
                t.Bounds.Written);
        }
        if constexpr(T::isFunction())
        {
            entries.emplace_back("return-type",
                domCreate(t.ReturnType, domCorpus));
            entries.emplace_back("param-types",
                dom::LazyArray(t.ParamTypes, [&](std::unique_ptr<TypeInfo> const& I)
                {
                    return domCreate(I, domCorpus);
                }));
            entries.emplace_back("exception-spec",
                toString(t.ExceptionSpec));
            entries.emplace_back("ref-qualifier",
                toString(t.RefQualifier));
            entries.emplace_back("is-variadic", t.IsVariadic);
        }
    });
    return dom::Object(std::move(entries));
}

//------------------------------------------------
//
// Interface
//
//------------------------------------------------

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

//------------------------------------------------
//
// Info
//
//------------------------------------------------

std::string_view
getDefaultAccess(
    RecordInfo const& I) noexcept
{
    switch(I.KeyKind)
    {
    case RecordKeyKind::Class:
        return "private";
    case RecordKeyKind::Struct:
    case RecordKeyKind::Union:
        return "public";
    default:
        MRDOCS_UNREACHABLE();
    }
}

//------------------------------------------------

} // (anon)

//------------------------------------------------

class DomCorpus::Impl
{
    using value_type = std::weak_ptr<dom::ObjectImpl>;

    DomCorpus const& domCorpus_;
    Corpus const& corpus_;
    std::unordered_map<SymbolID, value_type> cache_;
    std::mutex mutex_;

public:
    Impl(
        DomCorpus const& domCorpus,
        Corpus const& corpus)
        : domCorpus_(domCorpus)
        , corpus_(corpus)
    {
    }

    Corpus const&
    getCorpus() const
    {
        return corpus_;
    }

    dom::Object
    create(Info const& I)
    {
        return domCorpus_.construct(I);
    }

    dom::Object
    get(SymbolID const& id)
    {
        // VFALCO Hack to deal with symbol IDs
        // being emitted without the corresponding data.
        const Info* I = corpus_.find(id);
        if(! I)
            return {}; // VFALCO Hack

        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(id);
        if(it == cache_.end())
        {
            auto obj = create(*I);
            cache_.insert(
                { id, obj.impl() });
            return obj;
        }
        if(auto sp = it->second.lock())
            return dom::Object(sp);
        auto obj = create(*I);
        it->second = obj.impl();
        return obj;
    }
};

DomCorpus::
~DomCorpus() = default;

DomCorpus::
DomCorpus(Corpus const& corpus_)
    : impl_(std::make_unique<Impl>(
        *this, corpus_))
{
}

Corpus const&
DomCorpus::
getCorpus() const
{
    return impl_->getCorpus();
}

namespace dom {
    /* Determine if a type has a mrdocs::toString overload
     */
    template <class U>
    concept HasMrDocsToString = requires(U u)
    {
        { mrdocs::toString(u) } -> std::convertible_to<std::string_view>;
    };

    /* Convert enum Values to strings using mrdocs::toString
     */
    template <HasMrDocsToString U>
    requires std::is_enum_v<U>
    struct ToValue<U>
    {
        std::string_view
        operator()(U const& v) const
        {
            return mrdocs::toString(v);
        }
    };

    static_assert(HasToValue<InfoKind>);
    static_assert(HasToValue<AccessKind>);

    /* Convert SymbolID to strings using toBase16
     */
    template <>
    struct ToValue<SymbolID>
    {
        std::string
        operator()(SymbolID const& id) const
        {
            return toBase16(id);
        }
    };

    static_assert(HasToValue<SymbolID>);

    /* Mapping Traits for an Info type

       These traits map an Info type to a DOM object.
       It includes all members of the derived type.

       The traits store a reference to the DomCorpus
       so that it can resolve symbol IDs to the corresponding
       Info objects. Whenever a member refers to symbol IDs,
       the mapping trait will automatically resolve the
       symbol ID to the corresponding Info object.

       This allows all references to be resolved to the
       corresponding Info object lazily from the templates
       that use the DOM.
     */
    template<std::derived_from<Info> T>
    struct MappingTraits<T>
    {
    private:
        DomCorpus const* domCorpus_ = nullptr;

    public:
        MappingTraits(DomCorpus const& domCorpus)
            : domCorpus_(&domCorpus)
        {
        }

        /* Resolve types that use SymbolIDs to the corresponding Info object
         */
        template <class U>
        Value
        resolve(U const& I) const
        {
            if constexpr (std::same_as<U, SymbolID>)
            {
                return domCorpus_->get(I);
            }
            else if constexpr (std::same_as<Javadoc, U>)
            {
                return domCorpus_->getJavadoc(I);
            }
            else if constexpr (
                std::ranges::range<U> &&
                std::same_as<std::ranges::range_value_t<U>, SymbolID>)
            {
                return dom::LazyArray(I, [this](SymbolID const& id)
                    {
                        return domCorpus_->get(id);
                    });
            }
            else if constexpr (
                std::ranges::range<U> &&
                std::same_as<std::ranges::range_value_t<U>, Param>)
            {
                return dom::LazyArray(I, [&](Param const& p)
                    {
                        return dom::Object({
                            { "name", dom::stringOrNull(p.Name) },
                            { "type", domCreate(p.Type, *domCorpus_) },
                            { "default", dom::stringOrNull(p.Default) }
                        });
                    });
            }
            else
            {
                MRDOCS_UNREACHABLE();
            }
        }

        template <class IO>
        void
        map(IO &io, T const& I) const
        {
            MRDOCS_ASSERT(domCorpus_);
            io.map("id", I.id);
            if (!I.Name.empty())
            {
                io.map("name", I.Name);
            }
            io.map("kind", I.Kind);
            io.map("access", I.Access);
            io.map("implicit", I.Implicit);
            io.defer("namespace", [&]{
                return resolve(I.Namespace);
            });
            if (!I.Namespace.empty())
            {
                io.defer("parent",[&]{
                    return resolve(I.Namespace.front());
                });
            }
            if (I.javadoc)
            {
                io.defer("doc",[&]{
                    return resolve(*I.javadoc);
                });
            }
            if constexpr(std::derived_from<T, ScopeInfo>)
            {
                io.defer("members", [&]{
                    return resolve(I.Members);
                });
                io.defer("overloads",[&]{
                    return generateScopeOverloadsArray(I, *domCorpus_);
                });
            }
            if constexpr(std::derived_from<T, SourceInfo>)
            {
                io.map("loc", static_cast<SourceInfo const&>(I));
            }
            if constexpr(T::isNamespace())
            {
                io.defer("interface", [&]{
                    return dom::newObject<DomTranche>(
                        std::make_shared<Tranche>(
                            makeTranche(I, **domCorpus_)),
                        *domCorpus_);
                });
                io.defer("usingDirectives", [&]{
                    return resolve(I.UsingDirectives);
                });
            }
            if constexpr (T::isRecord())
            {
                io.map("tag", I.KeyKind);
                io.defer("defaultAccess", [&]{
                    return getDefaultAccess(I);
                });
                io.map("isTypedef", I.IsTypeDef);
                io.map("bases", dom::LazyArray(I.Bases, [&](BaseInfo const& I)
                    {
                        return dom::Object({
                            { "access", toString(I.Access) },
                            { "isVirtual", I.IsVirtual },
                            { "type", domCreate(I.Type, *domCorpus_) }
                        });
                    }));
                io.defer("interface", [&]{
                    auto sp = std::make_shared<Interface>(makeInterface(I, domCorpus_->getCorpus()));
                    return dom::Object({
                        { "public", dom::newObject<DomTranche>(sp->Public, *domCorpus_) },
                        { "protected", dom::newObject<DomTranche>(sp->Protected, *domCorpus_) },
                        { "private", dom::newObject<DomTranche>(sp->Private, *domCorpus_) }
                    });
                });
                io.defer("template", [&]{
                    return domCreate(I.Template, *domCorpus_);
                });
            }
            if constexpr (T::isEnum())
            {
                io.defer("type", [&]{
                    return domCreate(I.UnderlyingType, *domCorpus_);
                });
                io.map("isScoped", I.Scoped);
            }
            if constexpr (T::isFunction())
            {
                io.map("isVariadic", I.IsVariadic);
                io.map("isVirtual", I.IsVirtual);
                io.map("isVirtualAsWritten", I.IsVirtualAsWritten);
                io.map("isPure", I.IsPure);
                io.map("isDefaulted", I.IsDefaulted);
                io.map("isExplicitlyDefaulted", I.IsExplicitlyDefaulted);
                io.map("isDeleted", I.IsDeleted);
                io.map("isDeletedAsWritten", I.IsDeletedAsWritten);
                io.map("isNoReturn", I.IsNoReturn);
                io.map("hasOverrideAttr", I.HasOverrideAttr);
                io.map("hasTrailingReturn", I.HasTrailingReturn);
                io.map("isConst", I.IsConst);
                io.map("isVolatile", I.IsVolatile);
                io.map("isFinal", I.IsFinal);
                io.map("isNodiscard", I.IsNodiscard);
                io.map("isExplicitObjectMemberFunction", I.IsExplicitObjectMemberFunction);
                if (I.Constexpr != ConstexprKind::None)
                {
                    io.map("constexprKind", I.Constexpr);
                }
                if (I.StorageClass != StorageClassKind::None)
                {
                    io.map("storageClass", I.StorageClass);
                }
                if (I.RefQualifier != ReferenceKind::None)
                {
                    io.map("refQualifier", I.RefQualifier);
                }
                io.map("class", I.Class);
                io.defer("params", [&]{ return resolve(I.Params); });
                io.defer("return", [&]{ return domCreate(I.ReturnType, *domCorpus_); });
                io.defer("template", [&]{ return domCreate(I.Template, *domCorpus_); });
                io.map("overloadedOperator", I.OverloadedOperator);
                io.defer("exceptionSpec", [&]{ return toString(I.Noexcept); });
                io.defer("explicitSpec", [&]{ return toString(I.Explicit); });
                if (!I.Requires.Written.empty())
                {
                    io.map("requires", I.Requires.Written);
                }
            }
            if constexpr (T::isTypedef())
            {
                io.defer("type", [&]{
                    return domCreate(I.Type, *domCorpus_);
                });
                io.defer("template", [&]{
                    return domCreate(I.Template, *domCorpus_);
                });
                io.map("isUsing", I.IsUsing);
            }
            if constexpr (T::isVariable())
            {
                io.defer("type", [&]{
                    return domCreate(I.Type, *domCorpus_);
                });
                io.defer("template", [&]{
                    return domCreate(I.Template, *domCorpus_);
                });
                if (I.Constexpr != ConstexprKind::None)
                {
                    io.map("constexprKind", I.Constexpr);
                }
                if (I.StorageClass != StorageClassKind::None)
                {
                    io.map("storageClass", I.StorageClass);
                }
                io.map("isConstinit", I.IsConstinit);
                io.map("isThreadLocal", I.IsThreadLocal);
                if (!I.Initializer.Written.empty())
                {
                    io.map("initializer", I.Initializer.Written);
                }
            }
            if constexpr (T::isField())
            {
                io.defer("type", [&]{
                    return domCreate(I.Type, *domCorpus_);
                });
                if (!I.Default.Written.empty())
                {
                    io.map("default", I.Default.Written);
                }
                io.map("isMaybeUnused", I.IsMaybeUnused);
                io.map("isDeprecated", I.IsDeprecated);
                io.map("isVariant", I.IsVariant);
                io.map("isMutable", I.IsMutable);
                io.map("isBitfield", I.IsBitfield);
                io.map("hasNoUniqueAddress", I.HasNoUniqueAddress);
                if (I.IsBitfield)
                {
                    io.map("bitfieldWidth", I.BitfieldWidth.Written);
                }
            }
            if constexpr (T::isSpecialization())
            {}
            if constexpr (T::isFriend())
            {
                if (I.FriendSymbol)
                {
                    io.defer("name", [&]{
                        return domCorpus_->get(I.FriendSymbol).get("name");
                    });
                    io.defer("symbol", [&]{
                        return domCorpus_->get(I.FriendSymbol);
                    });
                }
                else if (I.FriendType)
                {
                    io.defer("name", [&]{
                        return domCreate(I.FriendType, *domCorpus_).get("name");
                    });
                    io.defer("type", [&]{
                        return domCreate(I.FriendType, *domCorpus_);
                    });
                }
            }
            if constexpr (T::isAlias())
            {
                MRDOCS_ASSERT(I.AliasedSymbol);
                io.defer("aliasedSymbol", [&]{
                    return domCreate(I.AliasedSymbol, *domCorpus_);
                });
            }
            if constexpr (T::isUsing())
            {
                io.map("class", I.Class);
                io.defer("shadows", [&]{
                    return resolve(I.UsingSymbols);
                });
                io.defer("qualifier", [&]{
                    return domCreate(I.Qualifier, *domCorpus_);
                });
            }
            if constexpr (T::isEnumerator())
            {
                if (!I.Initializer.Written.empty())
                {
                    io.map("initializer", I.Initializer.Written);
                }
            }
            if constexpr (T::isGuide())
            {
                io.defer("params", [&]{
                    return resolve(I.Params);
                });
                io.defer("deduced", [&]{
                    return domCreate(I.Deduced, *domCorpus_);
                });
                io.defer("template", [&]{
                    return domCreate(I.Template, *domCorpus_);
                });
                io.defer("explicitSpec", [&]{
                    return toString(I.Explicit);
                });
            }
            if constexpr (T::isConcept())
            {
                io.defer("template", [&]{
                    return domCreate(I.Template, *domCorpus_);
                });
                if (!I.Constraint.Written.empty())
                {
                    io.map("constraint", I.Constraint.Written);
                }
            }
        }
    };
}

dom::Object
DomCorpus::
construct(Info const& I) const
{
    return visit(I,
        [&]<class T>(T const& I)
        {
            return dom::newObject<dom::LazyObjectImpl<T>>(I, dom::MappingTraits<T>(*this));
        });
}

dom::Value
DomCorpus::
get(SymbolID const& id) const
{
    if(! id)
        return nullptr;
    return impl_->get(id);
}

dom::Value
DomCorpus::
getJavadoc(
    Javadoc const& jd) const
{
    // Default implementation returns null.
    return nullptr;
}

dom::Object
DomCorpus::
getOverloads(
    OverloadSet const& overloads) const
{
    auto resolveFn = [this](SymbolID const& id)
    {
        return this->get(id);
    };
    return dom::Object({
        { "kind",       "overload"},
        { "name",       overloads.Name },
        { "members",    dom::LazyArray(overloads.Members, resolveFn) },
        { "namespace",  dom::LazyArray(overloads.Namespace, resolveFn) },
        { "parent",     this->get(overloads.Parent) }
    });
}

} // mrdocs
} // clang
