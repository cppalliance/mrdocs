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
#include "lib/Support/SafeNames.hpp"
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Metadata/DomMetadata.hpp>
#include <llvm/ADT/StringMap.h>
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

static
dom::Value
domCreate(
    std::unique_ptr<Javadoc> const& jd,
    DomCorpus const& domCorpus)
{
    if(! jd)
        return nullptr;
    return domCorpus.getJavadoc(*jd);
}

class DomSymbolArray : public dom::ArrayImpl
{
    std::span<const SymbolID> list_;
    DomCorpus const& domCorpus_;
    //SharedPtr<> ref_; // keep owner of list_ alive

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
        MRDOCS_ASSERT(i < list_.size());
        return domCorpus_.get(list_[i]);
    }
};

//------------------------------------------------

static
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

static
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

static
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
        { "args", dom::newArray<DomTArgArray>(I->Args, domCorpus) }
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

template<class T>
class DomTrancheArray : public dom::ArrayImpl
{
    std::span<T const* const> list_;
    std::shared_ptr<Interface> sp_;
    DomCorpus const& domCorpus_;

public:
    DomTrancheArray(
        std::span<T const* const> list,
        std::shared_ptr<Interface> const& sp,
        DomCorpus const& domCorpus)
        : list_(list)
        , sp_(sp)
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
        return domCorpus_.get(list_[i]->id);
    }
};

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
            { "namespaces",       init(tranche->Namespaces, domCorpus) },
            { "records",          init(tranche->Records, domCorpus) },
            { "functions",        init(tranche->Functions, domCorpus) },
            { "enums",            init(tranche->Enums, domCorpus) },
            { "types",            init(tranche->Types, domCorpus) },
            { "fields",           init(tranche->Fields, domCorpus) },
            { "staticfuncs",      init(tranche->StaticFunctions, domCorpus) },
            { "variables",        init(tranche->Variables, domCorpus) },
            { "friends",          init(tranche->Friends, domCorpus) },
            { "guides",           init(tranche->Guides, domCorpus) },
            { "overloads",        init(tranche->Overloads, domCorpus) },
            { "staticoverloads",  init(tranche->StaticOverloads, domCorpus) },
            { "aliases",          init(tranche->Aliases, domCorpus) },
            { "usings",           init(tranche->Usings, domCorpus) }

            })
        , tranche_(tranche)
        , domCorpus_(domCorpus)
    {
    }
};

class DomInterface : public dom::LazyObjectImpl
{
    RecordInfo const& I_;
    DomCorpus const& domCorpus_;
    std::shared_ptr<Interface> mutable sp_;

public:
    DomInterface(
        RecordInfo const& I,
        DomCorpus const& domCorpus)
        : I_(I)
        , domCorpus_(domCorpus)
    {
    }

    dom::Object
    construct() const override
    {
        sp_ = std::make_shared<Interface>(makeInterface(I_, *domCorpus_));
        return dom::Object({
            { "public", dom::newObject<DomTranche>(sp_->Public, domCorpus_) },
            { "protected", dom::newObject<DomTranche>(sp_->Protected, domCorpus_) },
            { "private", dom::newObject<DomTranche>(sp_->Private, domCorpus_) },
            // { "overloads", dom::newArray<DomOverloadsArray>(sp_->Overloads, domCorpus_) },
            // { "static-overloads", dom::newArray<DomOverloadsArray>(sp_->StaticOverloads, domCorpus_) }
            });
    }
};

//------------------------------------------------
//
// Info
//
//------------------------------------------------

static
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

template<class T>
requires std::derived_from<T, Info>
class DomInfo : public dom::LazyObjectImpl
{
    T const& I_;
    DomCorpus const& domCorpus_;

public:
    DomInfo(
        T const& I,
        DomCorpus const& domCorpus) noexcept
        : I_(I)
        , domCorpus_(domCorpus)
    {
    }

    dom::Object construct() const override;
};

template<class T>
requires std::derived_from<T, Info>
dom::Object
DomInfo<T>::construct() const
{
    storage_type entries;
    entries.insert(entries.end(), {
        { "id",         toBase16(I_.id) },
        { "kind",       toString(I_.Kind) },
        { "access",     toString(I_.Access) },
        { "implicit",   I_.Implicit },
        { "namespace",  dom::newArray<DomSymbolArray>(
                            I_.Namespace, domCorpus_) },
        { "doc",        domCreate(I_.javadoc, domCorpus_) }
        });
    if(! I_.Name.empty())
        entries.emplace_back("name", I_.Name);
    if(! I_.Namespace.empty())
        entries.emplace_back("parent",
            domCorpus_.get(I_.Namespace.front()));

    if constexpr(std::derived_from<T, ScopeInfo>)
    {
        entries.insert(entries.end(), {
            { "members",   dom::newArray<DomSymbolArray>(I_.Members, domCorpus_) },
            { "overloads", dom::newArray<DomOverloadsArray>(I_, domCorpus_)},
            });
    }
    if constexpr(std::derived_from<T, SourceInfo>)
    {
        entries.emplace_back("loc", domCreate(I_));
    }
    if constexpr(T::isNamespace())
    {
        entries.emplace_back("interface", dom::newObject<DomTranche>(
            std::make_shared<Tranche>(
                makeTranche(I_, *domCorpus_)),
            domCorpus_));
    }
    if constexpr(T::isRecord())
    {
        entries.insert(entries.end(), {
            { "tag",            toString(I_.KeyKind) },
            { "defaultAccess",  getDefaultAccess(I_) },
            { "isTypedef",      I_.IsTypeDef },
            { "bases",          dom::newArray<DomBaseArray>(I_.Bases, domCorpus_) },
            { "interface",      dom::newObject<DomInterface>(I_, domCorpus_) },
            { "template",       domCreate(I_.Template, domCorpus_) }
            });
    }
    if constexpr(T::isEnum())
    {
        entries.insert(entries.end(), {
            { "type",       domCreate(I_.UnderlyingType, domCorpus_) },
            { "isScoped",   I_.Scoped }
            });
    }
    if constexpr(T::isFunction())
    {
        auto const set_flag =
            [&](dom::String key, bool set)
            {
                if(set)
                    entries.emplace_back(std::move(key), true);
            };
        set_flag("isVariadic",         I_.specs0.isVariadic.get());
        set_flag("isVirtual",          I_.specs0.isVirtual.get());
        set_flag("isVirtualAsWritten", I_.specs0.isVirtualAsWritten.get());
        set_flag("isPure",             I_.specs0.isPure.get());
        set_flag("isDefaulted",        I_.specs0.isDefaulted.get());
        set_flag("isExplicitlyDefaulted",I_.specs0.isExplicitlyDefaulted.get());
        set_flag("isDeleted",          I_.specs0.isDeleted.get());
        set_flag("isDeletedAsWritten", I_.specs0.isDeletedAsWritten.get());
        set_flag("isNoReturn",         I_.specs0.isNoReturn.get());
        set_flag("hasOverrideAttr",    I_.specs0.hasOverrideAttr.get());
        set_flag("hasTrailingReturn",  I_.specs0.hasTrailingReturn.get());
        set_flag("isConst",            I_.specs0.isConst.get());
        set_flag("isVolatile",         I_.specs0.isVolatile.get());
        set_flag("isFinal",            I_.specs0.isFinal.get());
        set_flag("isNodiscard",        I_.specs1.isNodiscard.get());
        set_flag("isExplicitObjectMemberFunction", I_.specs1.isExplicitObjectMemberFunction.get());

        auto const set_string =
            [&](dom::String key, dom::String value)
            {
                if(! value.empty())
                    entries.emplace_back(std::move(key), std::move(value));
            };
        set_string("constexprKind", toString(I_.specs0.constexprKind.get()));
        set_string("exceptionSpec", toString(I_.specs0.exceptionSpec.get()));
        set_string("storageClass",  toString(I_.specs0.storageClass.get()));
        set_string("refQualifier",  toString(I_.specs0.refQualifier.get()));

        entries.insert(entries.end(), {
            { "class",      toString(I_.Class) },
            { "params",     dom::newArray<DomParamArray>(I_.Params, domCorpus_) },
            { "return",     domCreate(I_.ReturnType, domCorpus_) },
            { "template",   domCreate(I_.Template, domCorpus_) },
            { "overloadedOperator", I_.specs0.overloadedOperator.get() },
            });

        entries.emplace_back("exceptionSpec", toString(I_.Noexcept));
        entries.emplace_back("explicitSpec",  toString(I_.Explicit));
        #if 0
        if(I_.Noexcept.Kind != NoexceptKind::None)
        {
            dom::String exceptSpec = "noexcept";
            if(! I_.Noexcept.Operand.empty())
                exceptSpec = fmt::format(
                    "noexcept({})",
                    I_.Noexcept.Operand);
            entries.emplace_back("exceptionSpec", std::move(exceptSpec));
        }
        #endif
    }
    if constexpr(T::isTypedef())
    {
        entries.insert(entries.end(), {
            { "type",       domCreate(I_.Type, domCorpus_) },
            { "template",   domCreate(I_.Template, domCorpus_) },
            { "isUsing",    I_.IsUsing }
            });
    }
    if constexpr(T::isVariable())
    {
        entries.insert(entries.end(), {
            { "type",           domCreate(I_.Type, domCorpus_) },
            { "template",       domCreate(I_.Template, domCorpus_) },
            { "constexprKind",  toString(I_.specs.constexprKind.get()) },
            { "storageClass",   toString(I_.specs.storageClass.get()) },
            { "isConstinit",    I_.specs.isConstinit.get() },
            { "isThreadLocal",  I_.specs.isThreadLocal.get() },
            { "initializer",    dom::stringOrNull(I_.Initializer.Written) },
            });
    }
    if constexpr(T::isField())
    {
        entries.insert(entries.end(), {
            { "type",               domCreate(I_.Type, domCorpus_) },
            { "default",            dom::stringOrNull(I_.Default.Written) },
            { "isMaybeUnused",      I_.specs.isMaybeUnused.get() },
            { "isDeprecated",       I_.specs.isDeprecated.get() },
            { "isMutable",          I_.IsMutable },
            { "isBitfield",         I_.IsBitfield },
            { "hasNoUniqueAddress", I_.specs.hasNoUniqueAddress.get() }
            });
        if(I_.IsBitfield)
            entries.emplace_back("bitfieldWidth",
                I_.BitfieldWidth.Written);
    }
    if constexpr(T::isSpecialization())
    {
    }
    if constexpr(T::isFriend())
    {
        if(I_.FriendSymbol)
        {
            auto befriended = domCorpus_.get(I_.FriendSymbol);
            entries.emplace_back("name", befriended.get("name"));
            entries.emplace_back("symbol", befriended);
        }
        else if(I_.FriendType)
        {
            auto befriended = domCreate(I_.FriendType, domCorpus_);
            entries.emplace_back("name", befriended.get("name"));
            entries.emplace_back("type", befriended);
        }
    }
    if constexpr(T::isAlias())
    {
        MRDOCS_ASSERT(I_.AliasedSymbol);
        entries.emplace_back("aliasedSymbol", domCreate(I_.AliasedSymbol, domCorpus_));
    }
    if constexpr(T::isUsing())
    {
        entries.emplace_back("class", toString(I_.Class));
        entries.emplace_back("symbols", dom::newArray<DomSymbolArray>(I_.UsingSymbols, domCorpus_));
        if (I_.Qualifier)
        {
            entries.emplace_back("qualifier", domCreate(I_.Qualifier, domCorpus_));
        }
    }
    if constexpr(T::isEnumerator())
    {
        entries.insert(entries.end(), {
            { "initializer", dom::stringOrNull(I_.Initializer.Written) }
            });
    }
    if constexpr(T::isGuide())
    {
        entries.insert(entries.end(), {
            { "params",     dom::newArray<DomParamArray>(I_.Params, domCorpus_) },
            { "deduced",     domCreate(I_.Deduced, domCorpus_) },
            { "template",   domCreate(I_.Template, domCorpus_) }
            });

        entries.emplace_back("explicitSpec", toString(I_.Explicit));
    }
    return dom::Object(std::move(entries));
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

dom::Object
DomCorpus::
construct(Info const& I) const
{
    return visit(I,
        [&]<class T>(T const& I)
        {
            return dom::newObject<DomInfo<T>>(I, *this);
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
