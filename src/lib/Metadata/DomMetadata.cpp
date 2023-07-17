//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "lib/Support/Radix.hpp"
#include <mrdox/Metadata.hpp>
#include <mrdox/Metadata/DomMetadata.hpp>
#include <llvm/ADT/StringMap.h>
#include <memory>
#include <mutex>

namespace clang {
namespace mrdox {

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
    std::vector<SymbolID> const& list_;
    DomCorpus const& domCorpus_;
    //SharedPtr<> ref_; // keep owner of list_ alive

public:
    DomSymbolArray(
        std::vector<SymbolID> const& list,
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
        MRDOX_ASSERT(i < list_.size());
        return domCorpus_.get(list_[i]);
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
        { "file", loc.Filename },
        { "line", loc.LineNumber}
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
        MRDOX_ASSERT(i < list_.size());
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
        MRDOX_ASSERT(i < list_.size());
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
        MRDOX_ASSERT(i < list_.size());
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
        MRDOX_ASSERT(i < list_.size());
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
                // KRYSTIAN NOTE: hack for missing SymbolIDs
                if(t.Template != SymbolID::zero && 
                    domCorpus.corpus.find(t.Template))
                    entries.emplace_back("template", 
                        toBase16(t.Template));
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
        { "primary", domCorpus.getOptional(*I->Primary) },
        { "params", dom::newArray<DomTParamArray>( I->Params, domCorpus) },
        { "args", dom::newArray<DomTArgArray>(I->Args, domCorpus) }
        });
}

//------------------------------------------------

static
dom::Value
domCreate(
    std::unique_ptr<TypeInfo> const& I,
    DomCorpus const& domCorpus)
{
    if(! I)
        return nullptr;
    dom::Object::storage_type entries = {
        { "kind", toString(I->Kind) }
    };
    visit(*I, [&]<typename T>(const T& t)
    {
        if constexpr(requires { t.Name; })
            entries.emplace_back("name", t.Name);

        if constexpr(requires { t.id; })
        {
            // VFALCO hack for missing symbols?
            if(t.id != SymbolID::zero && domCorpus.corpus.find(t.id))
                entries.emplace_back("id", toBase16(t.id));
        }

        if constexpr(T::isSpecialization())
            entries.emplace_back("template-args",
                dom::newArray<DomTArgArray>(t.TemplateArgs, domCorpus));

        if constexpr(requires { t.CVQualifiers; })
            entries.emplace_back("cv-qualifiers",
                toString(t.CVQualifiers));

        if constexpr(requires { t.ParentType; })
            entries.emplace_back("parent-type",
                domCreate(t.ParentType, domCorpus));

        if constexpr(requires { t.PointeeType; })
            entries.emplace_back("pointee-type",
                domCreate(t.PointeeType, domCorpus));

        if constexpr(T::isPack())
            entries.emplace_back("pattern-type",
                domCreate(t.PatternType, domCorpus));

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
// EnumValueInfo
//
//------------------------------------------------

class DomEnumValueArray : public dom::ArrayImpl
{
    std::vector<EnumValueInfo> const& list_;
    DomCorpus const& domCorpus_;

public:
    DomEnumValueArray(
        std::vector<EnumValueInfo> const& list,
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
        auto const& I = list_.at(i);
        return dom::Object({
            { "name", I.Name },
            { "value", I.Initializer.Value ?
                *I.Initializer.Value : dom::Value() },
            { "expr", I.Initializer.Written },
            { "doc", domCreate(I.javadoc, domCorpus_) }
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
    std::span<T const*> list_;
    std::shared_ptr<Interface> sp_;
    DomCorpus const& domCorpus_;

public:
    DomTrancheArray(
        std::span<T const*> list,
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
        MRDOX_ASSERT(i < list_.size());
        return domCorpus_.get(*list_[i]);
    }
};

class DomTranche : public dom::DefaultObjectImpl
{
    std::shared_ptr<Interface> sp_;
    Interface::Tranche const& tranche_;
    DomCorpus const& domCorpus_;

    template<class T>
    static
    dom::Value
    init(
        std::span<T const*> list,
        std::shared_ptr<Interface> const& sp,
        DomCorpus const& domCorpus)
    {
        return dom::newArray<DomTrancheArray<T>>(list, sp, domCorpus);
    }

public:
    DomTranche(
        Interface::Tranche const& tranche,
        std::shared_ptr<Interface> const& sp,
        DomCorpus const& domCorpus) noexcept
        : dom::DefaultObjectImpl({
            { "records",    init(tranche.Records, sp, domCorpus) },
            { "functions",  init(tranche.Functions, sp, domCorpus) },
            { "enums",      init(tranche.Enums, sp, domCorpus) },
            { "types",      init(tranche.Types, sp, domCorpus) },
            { "field",      init(tranche.Data, sp, domCorpus) },
            { "staticfuncs",init(tranche.StaticFunctions, sp, domCorpus) },
            { "staticdata", init(tranche.StaticData, sp, domCorpus) }
            })
        , sp_(sp)
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
        sp_ = std::make_shared<Interface>(makeInterface(I_, domCorpus_.corpus));
        return dom::Object({
            { "public", dom::newObject<DomTranche>(sp_->Public, sp_, domCorpus_) },
            { "protected", dom::newObject<DomTranche>(sp_->Protected, sp_, domCorpus_) },
            { "private", dom::newObject<DomTranche>(sp_->Private, sp_, domCorpus_) }
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
        MRDOX_UNREACHABLE();
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
        { "name",       I_.Name },
        { "namespace",  dom::newArray<DomSymbolArray>(
                            I_.Namespace, domCorpus_) },
        { "doc",        domCreate(I_.javadoc, domCorpus_) }
        });
    if constexpr(std::derived_from<T, SourceInfo>)
    {
        entries.emplace_back("loc", domCreate(I_));
    }
    if constexpr(T::isNamespace())
    {
        entries.insert(entries.end(), {
            { "members", dom::newArray<DomSymbolArray>(
                I_.Members, domCorpus_) },
            { "specializations", nullptr }
            });
    }
    if constexpr(T::isRecord())
    {
        entries.insert(entries.end(), {
            { "tag",            toString(I_.KeyKind) },
            { "defaultAccess",  getDefaultAccess(I_) },
            { "isTypedef",      I_.IsTypeDef },
            { "bases",          dom::newArray<DomBaseArray>(I_.Bases, domCorpus_) },
            { "friends",        dom::newArray<DomSymbolArray>(I_.Friends, domCorpus_) },
            { "members",        dom::newArray<DomSymbolArray>(I_.Members, domCorpus_) },
            { "specializations",dom::newArray<DomSymbolArray>(I_.Specializations, domCorpus_) },
            { "interface",      dom::newObject<DomInterface>(I_, domCorpus_) },
            { "template",       domCreate(I_.Template, domCorpus_) }
            });
    }
    if constexpr(T::isFunction())
    {
        auto const set_flag =
            [&](dom::String key, bool set)
            {
                if(set)
                    entries.emplace_back(std::move(key), true);
            });
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

        auto const set_string =
            [&](dom::String key, dom::String value)
            {
                if(! value.empty())
                    entries.emplace_back(std::move(key), std::move(value));
            });
        set_string("constexprKind", toString(I_.specs0.constexprKind.get()));
        set_string("exceptionSpec", toString(I_.specs0.exceptionSpec.get()));
        set_string("storageClass",  toString(I_.specs0.storageClass.get()));
        set_string("refQualifier",  toString(I_.specs0.refQualifier.get()));
        set_string("explicitSpec",  toString(I_.specs1.explicitSpec.get()));

        entries.insert(entries.end(), {
            { "class",      toString(I_.Class) },
            { "params",     dom::newArray<DomParamArray>(I_.Params, domCorpus_) },
            { "return",     domCreate(I_.ReturnType, domCorpus_) },
            { "template",   domCreate(I_.Template, domCorpus_) },
            { "overloadedOperator", I_.specs0.overloadedOperator.get() },
            });
    }
    if constexpr(T::isEnum())
    {
        entries.insert(entries.end(), {
            { "type",       domCreate(I_.UnderlyingType, domCorpus_) },
            { "members",    dom::newArray<DomEnumValueArray>(I_.Members, domCorpus_) },
            { "isScoped",   I_.Scoped }
            });
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
            { "storageClass",   toString(I_.specs.storageClass) }
            });
    }
    if constexpr(T::isField())
    {
        entries.insert(entries.end(), {
            { "type",           domCreate(I_.Type, domCorpus_) },
            { "default",        dom::stringOrNull(I_.Default) },
            { "isMaybeUnused",  I_.specs.isMaybeUnused.get() },
            { "isDeprecated",   I_.specs.isDeprecated.get() },
            { "hasNoUniqueAddress", I_.specs.hasNoUniqueAddress.get() }
            });
    }
    if constexpr(T::isSpecialization())
    {
    }
    return dom::Object(std::move(entries));
}

//------------------------------------------------

} // (anon)

//------------------------------------------------

class DomCorpus::Impl
{
    struct value_type
    {
        std::weak_ptr<dom::ObjectImpl> weak;
        std::shared_ptr<dom::ObjectImpl> strong;
    };

    DomCorpus const& domCorpus_;
    Corpus const& corpus_;
    llvm::StringMap<value_type> infoCache_;
    std::mutex mutex_;

public:
    Impl(
        DomCorpus const& domCorpus,
        Corpus const& corpus) noexcept
        : domCorpus_(domCorpus)
        , corpus_(corpus)
    {
    }

    dom::Object
    create(SymbolID const& id)
    {
        // VFALCO Hack to deal with symbol IDs
        // being emitted without the corresponding data.
        auto I = corpus_.find(id);
        if(I)
        {
            return visit(*I,
                [&]<class T>(T const& I)
                {
                    return dom::newObject<DomInfo<T>>(I, domCorpus_);
                });
        }
        return {}; // VFALCO Hack
    }

    dom::Object
    get(Info const& I)
    {
        return visit(I,
            [&]<class T>(T const& I)
            {
                return dom::newObject<DomInfo<T>>(I, domCorpus_);
            });
    }

    dom::Object
    get(SymbolID const& id)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = infoCache_.find(llvm::StringRef(id));
        if(it == infoCache_.end())
        {
            auto obj = create(id);
            auto impl = obj.impl();
            infoCache_.insert(
                { llvm::StringRef(id), { impl, nullptr } });
            return obj;
        }
        if(it->second.strong)
            return dom::Object(it->second.strong);
        auto sp = it->second.weak.lock();
        if(sp)
            return dom::Object(sp);
        auto obj = create(id);
        it->second.weak = obj.impl();
        return obj;
    }
};

DomCorpus::
~DomCorpus() = default;

DomCorpus::
DomCorpus(
    Corpus const& corpus_)
    : impl_(std::make_unique<Impl>(*this, corpus_))
    , corpus(corpus_)
{
}

dom::Object
DomCorpus::
get(SymbolID const& id) const
{
    return impl_->get(id);
}

dom::Object
DomCorpus::
get(Info const& I) const
{
    return impl_->get(I);
}

dom::Value
DomCorpus::
getOptional(SymbolID const& id) const
{
    if(id == SymbolID::zero)
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

} // mrdox
} // clang
