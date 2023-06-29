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

#include "Support/Radix.hpp"
#include <mrdox/Metadata.hpp>
#include <mrdox/Metadata/DomMetadata.hpp>
#include <mrdox/Support/SharedPtr.hpp>
#include <llvm/ADT/StringMap.h>
#include <mutex>
#include "-adoc/DocVisitor.hpp" // VFALCO NO!

namespace clang {
namespace mrdox {

class DomStyle
{
public:
    virtual ~DomStyle() = 0;
    virtual dom::Value createJavadoc(Javadoc const&) const = 0;
};

namespace {

//------------------------------------------------

/** Return a Dom node for the given metadata.

    If `id` is equal to @ref SymbolID::zero, a
    null value is returned.
*/
inline
dom::Value
domCreateInfoOrNull(
    SymbolID const& id,
    Corpus const& corpus)
{
    if(id != SymbolID::zero)
        return domCreateInfo(id, corpus);
    return nullptr;
}

template<class T>
dom::Value
domEagerCreateInfo(
    T const* I,
    Corpus const& corpus);

//------------------------------------------------
//
// Helpers
//
//------------------------------------------------

class DomSymbolArray : public dom::ArrayImpl
{
    std::vector<SymbolID> const& list_;
    Corpus const& corpus_;
    //SharedPtr<> ref_; // keep owner of list_ alive

public:
    DomSymbolArray(
        std::vector<SymbolID> const& list,
        Corpus const& corpus) noexcept
        : list_(list)
        , corpus_(corpus)
    {
    }

    std::size_t
    size() const noexcept override
    {
        return list_.size();
    }

    dom::Value
    at(std::size_t index) const override
    {
        return domCreateInfo(list_.at(index), corpus_);
    }
};

//------------------------------------------------
//
// Javadoc
//
//------------------------------------------------

class DomJavadoc : public dom::LazyObjectImpl
{
    Javadoc const& jd_;

public:
    DomJavadoc(
        Javadoc const& jd) noexcept
        : jd_(jd)
    {
    }

    dom::Object construct() const override
    {
        dom::ObjectImpl::entries_type list;
        list.reserve(2);

        // brief
        if(auto brief = jd_.getBrief())
        {
            std::string s;
            adoc::DocVisitor visitor(s);
            s.clear();
            visitor(*brief);
            if(! s.empty())
                list.emplace_back("brief", std::move(s));
        }

        // description
        if(! jd_.getBlocks().empty())
        {
            std::string s;
            adoc::DocVisitor visitor(s);
            s.clear();
            visitor(jd_.getBlocks());
            if(! s.empty())
                list.emplace_back("description", std::move(s));
        }

        return dom::Object(std::move(list));
    }
};

static
dom::Value
domCreate(
    std::unique_ptr<Javadoc> const& jd)
{
    if(jd)
        return dom::newObject<DomJavadoc>(*jd);
    return nullptr;
}

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

    dom::Value at(std::size_t i) const override
    {
        return domCreate(list_.at(i));
    }
};

static
dom::Object
domCreate(SourceInfo const& I)
{
    dom::Object::entries_type entries;
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

static dom::Value domCreate(std::unique_ptr<TypeInfo> const&, Corpus const&);

class DomTypeInfoArray : public dom::ArrayImpl
{
    std::vector<std::unique_ptr<TypeInfo>> const& list_;
    Corpus const& corpus_;

public:
    DomTypeInfoArray(
        std::vector<std::unique_ptr<TypeInfo>> const& list,
        Corpus const& corpus) noexcept
        : list_(list)
        , corpus_(corpus)
    {
    }

    std::size_t size() const noexcept override
    {
        return list_.size();
    }

    dom::Value at(std::size_t index) const override
    {
        MRDOX_ASSERT(index < list_.size());
        return domCreate(list_[index], corpus_);
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
    Corpus const& corpus_;

public:
    DomParamArray(
        std::vector<Param> const& list,
        Corpus const& corpus) noexcept
        : list_(list)
        , corpus_(corpus)
    {
    }

    std::size_t size() const noexcept override
    {
        return list_.size();
    }

    dom::Value at(std::size_t index) const override
    {
        MRDOX_ASSERT(index < list_.size());
        auto const& I = list_[index];
        return dom::Object({
            { "name", dom::stringOrNull(I.Name) },
            { "type", domCreate(I.Type, corpus_) },
            { "default", dom::stringOrNull(I.Default) }
            });
    }
};

//------------------------------------------------
//
// TemplateInfo
//
//------------------------------------------------

static dom::Value domCreate(TArg const&, Corpus const&);
static dom::Value domCreate(TParam const&, Corpus const&);
static dom::Value domCreate(
    std::unique_ptr<TemplateInfo> const& I, Corpus const&);
static dom::Value getTParamDefault(TParam const& I, Corpus const&);

//------------------------------------------------

/** An array of template arguments
*/
class DomTArgArray : public dom::ArrayImpl
{
    std::vector<TArg> const& list_;
    Corpus const& corpus_;

public:
    DomTArgArray(
        std::vector<TArg> const& list,
        Corpus const& corpus) noexcept
        : list_(list)
        , corpus_(corpus)
    {
    }

    std::size_t size() const noexcept override
    {
        return list_.size();
    }

    dom::Value at(std::size_t index) const override
    {
        return domCreate(list_.at(index), corpus_);
    }
};

/** An array of template parameters
*/
class DomTParamArray : public dom::ArrayImpl
{
    std::vector<TParam> const& list_;
    Corpus const& corpus_;

public:
    DomTParamArray(
        std::vector<TParam> const& list,
        Corpus const& corpus) noexcept
        : list_(list)
        , corpus_(corpus)
    {
    }

    std::size_t size() const noexcept override
    {
        return list_.size();
    }

    dom::Value at(std::size_t index) const override
    {
        return domCreate(list_.at(index), corpus_);
    }
};

//------------------------------------------------

static
dom::Value
domCreate(
    TArg const& I, Corpus const& corpus)
{
    return dom::Object({
        { "value", dom::stringOrNull(I.Value) }
    });
}

static
dom::Value
domCreate(
    TParam const& I,
    Corpus const& corpus)
{
    return dom::Object({
        { "kind", toString(I.Kind)},
        { "name", dom::stringOrNull(I.Name)},
        { "is-pack", I.IsParameterPack},
        { "type", I.Kind == TParamKind::NonType
            ? domCreate(I.get<NonTypeTParam>().Type, corpus)
            : dom::Value()},
        { "params", I.Kind == TParamKind::Template
            ? dom::newArray<DomTParamArray>(
                I.get<TemplateTParam>().Params, corpus)
            : dom::Value()},
        { "default", getTParamDefault(I, corpus) }
    });
}

static
dom::Value
domCreate(
    std::unique_ptr<TemplateInfo> const& I,
    Corpus const& corpus)
{
    if(I)
        return dom::Object({
            { "kind", toString(I->specializationKind()) },
            { "primary", domCreateInfoOrNull( *I->Primary, corpus) },
            { "params", dom::newArray<DomTParamArray>( I->Params, corpus) },
            { "args", dom::newArray<DomTArgArray>(I->Args, corpus) }
            });
    return nullptr;
}

static
dom::Value
getTParamDefault(
    TParam const& I,
    Corpus const& corpus)
{
    switch(I.Kind)
    {
    case TParamKind::Type:
        return domCreate(I.get<TypeTParam>().Default, corpus);
    case TParamKind::NonType:
        return dom::Value(I.get<NonTypeTParam>().Default);
    case TParamKind::Template:
        return dom::Value(I.get<TemplateTParam>().Default);
    default:
        MRDOX_UNREACHABLE();
    }
}

//------------------------------------------------

static
dom::Value
domCreate(
    std::unique_ptr<TypeInfo> const& I,
    Corpus const& corpus)
{
    if(! I)
        return nullptr;
    dom::Object::entries_type entries = {
        { "kind", toString(I->Kind) }
    };
    visit(*I, [&]<typename T>(const T& t)
    {
        if constexpr(requires { t.Name; })
            entries.emplace_back("name", t.Name);

        if constexpr(requires { t.id; })
        {
            // VFALCO hack for missing symbols?
            if(t.id != SymbolID::zero && corpus.find(t.id))
                entries.emplace_back("id", toBase16(t.id));
        }

        if constexpr(T::isSpecialization())
            entries.emplace_back("template-args",
                dom::newArray<DomTArgArray>(t.TemplateArgs, corpus));

        if constexpr(requires { t.CVQualifiers; })
            entries.emplace_back("cv-qualifiers",
                toString(t.CVQualifiers));

        if constexpr(requires { t.ParentType; })
            entries.emplace_back("parent-type",
                domCreate(t.ParentType, corpus));

        if constexpr(requires { t.PointeeType; })
            entries.emplace_back("pointee-type",
                domCreate(t.PointeeType, corpus));

        if constexpr(T::isPack())
            entries.emplace_back("pattern-type",
                domCreate(t.PatternType, corpus));

        if constexpr(T::isArray())
        {
            entries.emplace_back("element-type",
                domCreate(t.ElementType, corpus));
            entries.emplace_back("bounds-value",
                t.BoundsValue);
            entries.emplace_back("bounds-expr",
                t.BoundsExpr);
        }

        if constexpr(T::isFunction())
        {
            entries.emplace_back("return-type",
                domCreate(t.ReturnType, corpus));
            entries.emplace_back("param-types",
                dom::newArray<DomTypeInfoArray>(t.ParamTypes, corpus));
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
    Corpus const& corpus_;

public:
    DomBaseArray(
        std::vector<BaseInfo> const& list,
        Corpus const& corpus) noexcept
        : list_(list)
        , corpus_(corpus)
    {
    }

    std::size_t size() const noexcept override
    {
        return list_.size();
    }

    dom::Value at(std::size_t index) const override
    {
        auto const& I = list_.at(index);
        return dom::Object({
            { "access", toString(I.Access) },
            { "isVirtual", I.IsVirtual },
            { "type", domCreate(I.Type, corpus_) }
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

public:
    explicit
    DomEnumValueArray(
        std::vector<EnumValueInfo> const& list) noexcept
        : list_(list)
    {
    }

    std::size_t size() const noexcept override
    {
        return list_.size();
    }

    dom::Value at(std::size_t i) const override
    {
        auto const& I = list_.at(i);
        return dom::Object({
            { "name", I.Name },
            { "value", I.Value },
            { "expr", I.ValueExpr },
            { "doc", domCreate(I.javadoc) }
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

public:
    DomTrancheArray(
        std::span<T const*> list,
        std::shared_ptr<Interface> const& sp)
        : list_(list)
        , sp_(sp)
    {
    }

    std::size_t size() const noexcept override
    {
        return list_.size();
    }

    dom::Value
    at(std::size_t index) const override
    {
        if(index < list_.size())
            return domEagerCreateInfo(
                *list_[index], sp_->corpus);
        throw std::out_of_range("index");
    }
};

class DomTranche : public dom::DefaultObjectImpl
{
    std::shared_ptr<Interface> sp_;
    Interface::Tranche const& tranche_;

    template<class T>
    static
    dom::Value
    init(
        std::span<T const*> list,
        std::shared_ptr<Interface> const& sp)
    {
        return dom::newArray<DomTrancheArray<T>>(list, sp);
    }

public:
    DomTranche(
        Interface::Tranche const& tranche,
        std::shared_ptr<Interface> const& sp) noexcept
        : dom::DefaultObjectImpl({
            { "records",    init(tranche.Records, sp) },
            { "functions",  init(tranche.Functions, sp) },
            { "enums",      init(tranche.Enums, sp) },
            { "types",      init(tranche.Types, sp) },
            { "field",      init(tranche.Data, sp) },
            { "staticfuncs",init(tranche.StaticFunctions, sp) },
            { "staticdata", init(tranche.StaticData, sp) }
            })
        , sp_(sp)
        , tranche_(tranche)
    {
    }
};

class DomInterface : public dom::LazyObjectImpl
{
    RecordInfo const& I_;
    Corpus const& corpus_;
    std::shared_ptr<Interface> mutable sp_;

public:
    DomInterface(
        RecordInfo const& I,
        Corpus const& corpus)
        : I_(I)
        , corpus_(corpus)
    {
    }

    dom::Object
    construct() const override
    {
        sp_ = std::make_shared<Interface>(makeInterface(I_, corpus_));
        return dom::Object({
            { "public", dom::newObject<DomTranche>(sp_->Public, sp_) },
            { "protected", dom::newObject<DomTranche>(sp_->Protected, sp_) },
            { "private", dom::newObject<DomTranche>(sp_->Private, sp_) }
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
    Corpus const& corpus_;

public:
    DomInfo(
        T const& I,
        Corpus const& corpus) noexcept
        : I_(I)
        , corpus_(corpus)
    {
    }

    dom::Object construct() const override;
};

template<class T>
requires std::derived_from<T, Info>
dom::Object
DomInfo<T>::construct() const
{
    entries_type entries;
    entries.insert(entries.end(), {
        { "id", toBase16(I_.id) },
        { "kind", toString(I_.Kind) },
        { "access", toString(I_.Access) },
        { "name", I_.Name },
        { "namespace", dom::newArray<DomSymbolArray>(
            I_.Namespace, corpus_) },
        { "doc", domCreate(I_.javadoc) }
        });
    if constexpr(std::derived_from<T, SourceInfo>)
    {
        entries.emplace_back("loc", domCreate(I_));
    }
    if constexpr(T::isNamespace())
    {
        entries.insert(entries.end(), {
            { "members", dom::newArray<DomSymbolArray>(
                I_.Members, corpus_) },
            { "specializations", nullptr }
            });
    }
    if constexpr(T::isRecord())
    {
        entries.insert(entries.end(), {
            { "tag",            toString(I_.KeyKind) },
            { "defaultAccess",  getDefaultAccess(I_) },
            { "isTypedef",      I_.IsTypeDef },
            { "bases",          dom::newArray<DomBaseArray>(I_.Bases, corpus_) },
            { "friends",        dom::newArray<DomSymbolArray>(I_.Friends, corpus_) },
            { "members",        dom::newArray<DomSymbolArray>(I_.Members, corpus_) },
            { "specializations",dom::newArray<DomSymbolArray>(I_.Specializations, corpus_) },
            { "interface",      dom::newObject<DomInterface>(I_, corpus_) },
            { "template",       domCreate(I_.Template, corpus_) }
            });
    }
    if constexpr(T::isFunction())
    {
        entries.insert(entries.end(), {
            { "params",     dom::newArray<DomParamArray>(I_.Params, corpus_) },
            { "return",     domCreate(I_.ReturnType, corpus_) },
            { "template",   domCreate(I_.Template, corpus_) },

            { "isVariadic",         I_.specs0.isVariadic.get() },
            { "isVirtual",          I_.specs0.isVirtual.get() },
            { "isVirtualAsWritten", I_.specs0.isVirtualAsWritten.get() },
            { "isPure",             I_.specs0.isPure.get() },
            { "isDefaulted",        I_.specs0.isDefaulted.get() },
          { "isExplicitlyDefaulted",I_.specs0.isExplicitlyDefaulted.get() },
            { "isDeleted",          I_.specs0.isDeleted.get() },
            { "isDeletedAsWritten", I_.specs0.isDeletedAsWritten.get() },
            { "isNoReturn",         I_.specs0.isNoReturn.get() },
            { "hasOverrideAttr",    I_.specs0.hasOverrideAttr.get() },
            { "hasTrailingReturn",  I_.specs0.hasTrailingReturn.get() },
            { "isConst",            I_.specs0.isConst.get() },
            { "isVolatile",         I_.specs0.isVolatile.get() },
            { "isFinal",            I_.specs0.isFinal.get() },
            { "isNodiscard",        I_.specs1.isNodiscard.get() },

            { "constexprKind",      toString(I_.specs0.constexprKind.get()) },
            { "exceptionSpec",      toString(I_.specs0.exceptionSpec.get()) },
            { "overloadedOperator", I_.specs0.overloadedOperator.get() },
            { "storageClass",       toString(I_.specs0.storageClass.get()) },
            { "refQualifier",       toString(I_.specs0.refQualifier.get()) },
            { "explicitSpec",       toString(I_.specs1.explicitSpec.get()) }
            });
    }
    if constexpr(T::isEnum())
    {
        entries.insert(entries.end(), {
            { "type", domCreate(I_.BaseType, corpus_) },
            { "members", dom::newArray<DomEnumValueArray>(I_.Members) },
            { "isScoped", I_.Scoped }
            });
    }
    if constexpr(T::isTypedef())
    {
        entries.insert(entries.end(), {
            { "type", domCreate(I_.Underlying, corpus_) },
            { "template", domCreate(I_.Template, corpus_) },
            { "isUsing", I_.IsUsing }
            });
    }
    if constexpr(T::isVariable())
    {
        entries.insert(entries.end(), {
            { "type", domCreate(I_.Type, corpus_) },
            { "template", domCreate(I_.Template, corpus_) },
            { "storageClass", toString(I_.specs.storageClass) }
            });
    }
    if constexpr(T::isField())
    {
        entries.insert(entries.end(), {
            { "type", domCreate(I_.Type, corpus_) },
            { "default", dom::stringOrNull(I_.Default) },
            { "isMaybeUnused", I_.specs.isMaybeUnused.get() },
            { "isDeprecated", I_.specs.isDeprecated.get() },
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

// Return a possibly-lazy Info node
dom::Value
domCreateInfo(
    Info const& I,
    Corpus const& corpus)
{
    return visit(I,
        [&corpus]<class T>(T const& I) ->
            dom::Value
        {
            return dom::newObject<DomInfo<T>>(I, corpus);
        });
}

// Return a never-lazy Info node
template<class T>
static
dom::Value
domEagerCreateInfo(
    T const& I,
    Corpus const& corpus)
{
    return DomInfo<T>(I, corpus).construct();
}

//------------------------------------------------

// The returned value can sometimes be lazy
dom::Object
domCreateInfo(
    SymbolID const& id,
    Corpus const& corpus)
{
    // VFALCO Hack to deal with symbol IDs
    // being emitted without the corresponding data.
    auto I = corpus.find(id);
    if(I)
        //return domCreateInfo(*I, corpus);
        return visit(*I,
            [&corpus]<class T>(T const& I)
            {
                return dom::newObject<DomInfo<T>>(I, corpus);
            });
    return {};
}

//------------------------------------------------

struct DomCorpus::Impl
{
    struct value_type
    {
        std::weak_ptr<dom::ObjectImpl> weak;
        std::shared_ptr<dom::ObjectImpl> strong;
    };

    llvm::StringMap<value_type> infoCache_;
    std::mutex mutex_;
};

DomCorpus::
~DomCorpus() = default;

DomCorpus::
DomCorpus(
    Corpus const& corpus_)
    : impl_(std::make_unique<Impl>())
    , corpus(corpus_)
{
}

dom::Object
DomCorpus::
get(SymbolID const& id)
{
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    auto it = impl_->infoCache_.find(llvm::StringRef(id));
    if(it == impl_->infoCache_.end())
    {
        auto obj = domCreateInfo(id, corpus);
        auto impl = obj.impl();
        impl_->infoCache_.insert(
            { llvm::StringRef(id), { impl, nullptr } });
        return obj;
    }
    if(it->second.strong)
        return dom::Object(it->second.strong);
    auto sp = it->second.weak.lock();
    if(sp)
        return dom::Object(sp);
    auto obj = domCreateInfo(id, corpus);
    it->second.weak = obj.impl();
    return obj;
}

} // mrdox
} // clang
