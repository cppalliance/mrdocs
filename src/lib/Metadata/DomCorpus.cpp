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

#include "lib/Support/Radix.hpp"
#include "lib/Support/LegibleNames.hpp"
#include "lib/Dom/LazyObject.hpp"
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

dom::Value
domCreate(
    std::unique_ptr<Javadoc> const& jd,
    DomCorpus const& domCorpus)
{
    if(!jd)
        return nullptr;
    return domCorpus.getJavadoc(*jd);
}

/* A Lazy DOM Array type that replaces symbol IDs with their
   corresponding DOM objects.
*/
class DomSymbolArray : public dom::ArrayImpl
{
    std::span<const SymbolID> list_;
    DomCorpus const& domCorpus_;

public:
    DomSymbolArray(
        std::span<const SymbolID> list,
        DomCorpus const& domCorpus) noexcept
        : list_(list)
        , domCorpus_(domCorpus)
    {
    }

    std::size_t size() const noexcept override
    {
        return list_.size();
    }

    dom::Value get(std::size_t i) const override
    {
        if (i < list_.size())
        {
            return domCorpus_.get(list_[i]);
        }
        return dom::Value{};
    }
};

//------------------------------------------------

dom::Object
domCreate(
    OverloadSet const& overloads,
    DomCorpus const& domCorpus)
{
    return dom::Object({
        { "kind",       "overload"},
        { "name",       overloads.Name },
        { "members",    dom::newArray<DomSymbolArray>(
            overloads.Members, domCorpus) },
        { "namespace",  dom::newArray<DomSymbolArray>(
            overloads.Namespace, domCorpus) },
        { "parent",     domCorpus.get(overloads.Parent) }
        });
}

class DomOverloadsArray : public dom::ArrayImpl
{
    std::vector<std::variant<
        SymbolID, OverloadSet>> overloads_;

    DomCorpus const& domCorpus_;

public:
    template<std::derived_from<ScopeInfo> ScopeInfoTy>
    DomOverloadsArray(
        const ScopeInfoTy& I,
        DomCorpus const& domCorpus) noexcept
        : domCorpus_(domCorpus)
    {
        overloads_.reserve(I.Lookups.size());
        domCorpus_->traverseOverloads(I,
            [&](const auto& C)
        {
            if constexpr(requires { C.id; })
                overloads_.emplace_back(C.id);
            else
                overloads_.emplace_back(C);
        });
    }

    std::size_t size() const noexcept override
    {
        return overloads_.size();
    }

    dom::Value get(std::size_t index) const override
    {
        MRDOCS_ASSERT(index < size());
        const auto& member = overloads_[index];
        if(auto* id = std::get_if<SymbolID>(&member))
            return domCorpus_.get(*id);
        return domCorpus_.getOverloads(
            std::get<OverloadSet>(member));
    }
};

//------------------------------------------------
//
// Location
// SourceInfo
//
//------------------------------------------------

dom::Object
domCreate(Location const& loc)
{
    return dom::Object({
        { "path",       loc.Path },
        { "file",       loc.Filename },
        { "line",       loc.LineNumber },
        { "kind",       toString(loc.Kind) },
        { "documented", loc.Documented }
        });
}

class DomLocationArray : public dom::ArrayImpl
{
    std::vector<Location> const& list_;

public:
    explicit
    DomLocationArray(
        std::vector<Location> const& list) noexcept
        : list_(list)
    {
    }

    std::size_t size() const noexcept override
    {
        return list_.size();
    }

    dom::Value get(std::size_t i) const override
    {
        MRDOCS_ASSERT(i < list_.size());
        return domCreate(list_[i]);
    }
};

dom::Object
domCreate(SourceInfo const& I)
{
    dom::Object::storage_type entries;
    if(I.DefLoc)
        entries.emplace_back("def", domCreate(*I.DefLoc));
    if(! I.Loc.empty())
        entries.emplace_back("decl", dom::newArray<DomLocationArray>(I.Loc));
    return dom::Object(std::move(entries));
}

//------------------------------------------------
//
// TypeInfo
//
//------------------------------------------------

static dom::Value domCreate(
    std::unique_ptr<TypeInfo> const&, DomCorpus const&);

class DomTypeInfoArray : public dom::ArrayImpl
{
    std::vector<std::unique_ptr<TypeInfo>> const& list_;
    DomCorpus const& domCorpus_;

public:
    DomTypeInfoArray(
        std::vector<std::unique_ptr<TypeInfo>> const& list,
        DomCorpus const& domCorpus) noexcept
        : list_(list)
        , domCorpus_(domCorpus)
    {
    }

    std::size_t size() const noexcept override
    {
        return list_.size();
    }

    dom::Value get(std::size_t i) const override
    {
        MRDOCS_ASSERT(i < list_.size());
        return domCreate(list_[i], domCorpus_);
    }
};

//------------------------------------------------
//
// Param
//
//------------------------------------------------

/** An array of function parameters
*/
class DomParamArray : public dom::ArrayImpl
{
    std::vector<Param> const& list_;
    DomCorpus const& domCorpus_;

public:
    DomParamArray(
        std::vector<Param> const& list,
        DomCorpus const& domCorpus) noexcept
        : list_(list)
        , domCorpus_(domCorpus)
    {
    }

    std::size_t size() const noexcept override
    {
        return list_.size();
    }

    dom::Value get(std::size_t i) const override
    {
        MRDOCS_ASSERT(i < list_.size());
        auto const& I = list_[i];
        return dom::Object({
            { "name", dom::stringOrNull(I.Name) },
            { "type", domCreate(I.Type, domCorpus_) },
            { "default", dom::stringOrNull(I.Default) }
            });
    }
};

//------------------------------------------------
//
// TemplateInfo
//
//------------------------------------------------

static dom::Value domCreate(
    std::unique_ptr<TArg> const&, DomCorpus const&);
static dom::Value domCreate(
    std::unique_ptr<TParam> const&, DomCorpus const&);
static dom::Value domCreate(
    std::unique_ptr<TemplateInfo> const& I, DomCorpus const&);
static dom::Value domCreate(
    std::unique_ptr<NameInfo> const& I, DomCorpus const&);

//------------------------------------------------

/** An array of template arguments
*/
class DomTArgArray : public dom::ArrayImpl
{
    std::vector<std::unique_ptr<TArg>> const& list_;
    DomCorpus const& domCorpus_;

public:
    DomTArgArray(
        std::vector<std::unique_ptr<TArg>> const& list,
        DomCorpus const& domCorpus) noexcept
        : list_(list)
        , domCorpus_(domCorpus)
    {
    }

    std::size_t size() const noexcept override
    {
        return list_.size();
    }

    dom::Value get(std::size_t i) const override
    {
        MRDOCS_ASSERT(i < list_.size());
        return domCreate(list_[i], domCorpus_);
    }
};

/** An array of template parameters
*/
class DomTParamArray : public dom::ArrayImpl
{
    std::vector<std::unique_ptr<TParam>> const& list_;
    DomCorpus const& domCorpus_;

public:
    DomTParamArray(
        std::vector<std::unique_ptr<TParam>> const& list,
        DomCorpus const& domCorpus) noexcept
        : list_(list)
        , domCorpus_(domCorpus)
    {
    }

    std::size_t size() const noexcept override
    {
        return list_.size();
    }

    dom::Value get(std::size_t i) const override
    {
        return domCreate(list_[i], domCorpus_);
    }
};

//------------------------------------------------

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
                    dom::newArray<DomTParamArray>(
                        t.Params, domCorpus));
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
        { "params", dom::newArray<DomTParamArray>( I->Params, domCorpus) },
        { "args", dom::newArray<DomTArgArray>(I->Args, domCorpus) },
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
                dom::newArray<DomTArgArray>(t.TemplateArgs, domCorpus));

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
                dom::newArray<DomTypeInfoArray>(t.ParamTypes, domCorpus));
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
// BaseInfo
//
//------------------------------------------------

class DomBaseArray : public dom::ArrayImpl
{
    std::vector<BaseInfo> const& list_;
    DomCorpus const& domCorpus_;

public:
    DomBaseArray(
        std::vector<BaseInfo> const& list,
        DomCorpus const& domCorpus) noexcept
        : list_(list)
        , domCorpus_(domCorpus)
    {
    }

    std::size_t size() const noexcept override
    {
        return list_.size();
    }

    dom::Value get(std::size_t i) const override
    {
        auto const& I = list_[i];
        return dom::Object({
            { "access", toString(I.Access) },
            { "isVirtual", I.IsVirtual },
            { "type", domCreate(I.Type, domCorpus_) }
            });
    }
};

//------------------------------------------------
//
// Interface
//
//------------------------------------------------

class DomTranche : public dom::DefaultObjectImpl
{
    std::shared_ptr<Tranche> tranche_;
    DomCorpus const& domCorpus_;

    static
    dom::Value
    init(
        std::span<const SymbolID> list,
        DomCorpus const& domCorpus)
    {
        return dom::newArray<DomSymbolArray>(list, domCorpus);
    }

    static
    dom::Value
    init(
        const ScopeInfo& scope,
        DomCorpus const& domCorpus)
    {
        return dom::newArray<DomOverloadsArray>(scope, domCorpus);
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
        , domCorpus_(domCorpus)
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

Corpus const&
DomCorpus::
operator*() const
{
    return getCorpus();
}

Corpus const*
DomCorpus::
operator->() const
{
    return &getCorpus();
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
            io.defer("namespace", [&]{ return dom::newArray<DomSymbolArray>(I.Namespace, *domCorpus_); });
            io.defer("doc", [&]{ return domCreate(I.javadoc, *domCorpus_); });
            if (!I.Namespace.empty())
            {
                io.defer("parent", [&]{ return domCorpus_->get(I.Namespace.front()); });
            }
            if constexpr(std::derived_from<T, ScopeInfo>)
            {
                io.defer("members", [&]{ return dom::newArray<DomSymbolArray>(I.Members, *domCorpus_); });
                io.defer("overloads", [&]{ return dom::newArray<DomOverloadsArray>(I, *domCorpus_); });
            }
            if constexpr(std::derived_from<T, SourceInfo>)
            {
                io.defer("loc", [&]{ return domCreate(I); });
            }
            if constexpr(T::isNamespace())
            {
                io.defer("interface", [&]{ return dom::newObject<DomTranche>(
                    std::make_shared<Tranche>(
                        makeTranche(I, **domCorpus_)),
                    *domCorpus_); });
                io.defer("usingDirectives", [&]{ return dom::newArray<DomSymbolArray>(
                    I.UsingDirectives, *domCorpus_); });
            }
            if constexpr (T::isRecord())
            {
                io.map("tag", I.KeyKind);
                io.defer("defaultAccess", [&]{ return getDefaultAccess(I); });
                io.map("isTypedef", I.IsTypeDef);
                io.defer("bases", [&]{ return dom::newArray<DomBaseArray>(I.Bases, *domCorpus_); });
                io.defer("interface", [&]{
                    auto sp = std::make_shared<Interface>(makeInterface(I, domCorpus_->getCorpus()));
                    return dom::Object({
                        { "public", dom::newObject<DomTranche>(sp->Public, *domCorpus_) },
                        { "protected", dom::newObject<DomTranche>(sp->Protected, *domCorpus_) },
                        { "private", dom::newObject<DomTranche>(sp->Private, *domCorpus_) },
                        // { "overloads", dom::newArray<DomOverloadsArray>(sp->Overloads, *domCorpus_) },
                        // { "static-overloads", dom::newArray<DomOverloadsArray>(sp->StaticOverloads, *domCorpus_) }
                    });
                });
                io.defer("template", [&]{ return domCreate(I.Template, *domCorpus_); });
            }
            if constexpr (T::isEnum())
            {
                io.defer("type", [&]{ return domCreate(I.UnderlyingType, *domCorpus_); });
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
                io.defer("params", [&]{ return dom::newArray<DomParamArray>(I.Params, *domCorpus_); });
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
                io.defer("type", [&]{ return domCreate(I.Type, *domCorpus_); });
                io.defer("template", [&]{ return domCreate(I.Template, *domCorpus_); });
                io.map("isUsing", I.IsUsing);
            }
            if constexpr (T::isVariable())
            {
                io.defer("type", [&]{ return domCreate(I.Type, *domCorpus_); });
                io.defer("template", [&]{ return domCreate(I.Template, *domCorpus_); });
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
                io.defer("type", [&]{ return domCreate(I.Type, *domCorpus_); });
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
                    io.defer("name", [&]{ return domCorpus_->get(I.FriendSymbol).get("name"); });
                    io.defer("symbol", [&]{ return domCorpus_->get(I.FriendSymbol); });
                }
                else if (I.FriendType)
                {
                    io.defer("name", [&]{ return domCreate(I.FriendType, *domCorpus_).get("name"); });
                    io.defer("type", [&]{ return domCreate(I.FriendType, *domCorpus_); });
                }
            }
            if constexpr (T::isAlias())
            {
                MRDOCS_ASSERT(I.AliasedSymbol);
                io.defer("aliasedSymbol", [&]{ return domCreate(I.AliasedSymbol, *domCorpus_); });
            }
            if constexpr (T::isUsing())
            {
                io.map("class", I.Class);
                io.defer("shadows", [&]{ return dom::newArray<DomSymbolArray>(I.UsingSymbols, *domCorpus_); });
                io.defer("qualifier", [&]{ return domCreate(I.Qualifier, *domCorpus_); });
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
                io.defer("params", [&]{ return dom::newArray<DomParamArray>(I.Params, *domCorpus_); });
                io.defer("deduced", [&]{ return domCreate(I.Deduced, *domCorpus_); });
                io.defer("template", [&]{ return domCreate(I.Template, *domCorpus_); });
                io.defer("explicitSpec", [&]{ return toString(I.Explicit); });
            }
            if constexpr (T::isConcept())
            {
                io.defer("template", [&]{ return domCreate(I.Template, *domCorpus_); });
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
    OverloadSet const& os) const
{
    return domCreate(os, *this);
}

} // mrdocs
} // clang
