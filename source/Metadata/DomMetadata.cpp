//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "Support/Radix.hpp"
#include <mrdox/Metadata.hpp>
#include <mrdox/Metadata/DomMetadata.hpp>

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

class DomSymbolArray : public dom::Array
{
    std::vector<SymbolID> const& list_;
    Corpus const& corpus_;
    //dom::Pointer<> ref_; // keep owner of list_ alive

public:
    DomSymbolArray(
        std::vector<SymbolID> const& list,
        Corpus const& corpus) noexcept
        : list_(list)
        , corpus_(corpus)
    {
    }

    std::size_t length() const noexcept override
    {
        return list_.size();
    }

    dom::Value get(std::size_t i) const override
    {
        if(i < list_.size())
            return domCreateInfo(list_[i], corpus_);
        return nullptr;
    }
};

//------------------------------------------------
//
// Javadoc
//
//------------------------------------------------

class DomJavadoc : public dom::LazyObject
{
    Javadoc const& jd_;

public:
    DomJavadoc(
        Javadoc const& jd) noexcept
        : jd_(jd)
    {
    }

    dom::ObjectPtr construct() const override
    {
        dom::Object::list_type list;
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

        return dom::create<dom::Object>(std::move(list));
    }
};

static
dom::Value
domCreate(
    std::unique_ptr<Javadoc> const& jd)
{
    if(jd)
        return dom::create<DomJavadoc>(*jd);
    return nullptr;
}

//------------------------------------------------
//
// Location
// SourceInfo
//
//------------------------------------------------

class DomLocation : public dom::Object
{
public:
    explicit
    DomLocation(
        Location const& loc) noexcept
        : Object({
            { "file", loc.Filename },
            { "line", loc.LineNumber} })
    {
    }
};

class DomLocationArray : public dom::Array
{
    std::vector<Location> const& list_;

public:
    explicit
    DomLocationArray(
        std::vector<Location> const& list) noexcept
        : list_(list)
    {
    }

    std::size_t length() const noexcept override
    {
        return list_.size();
    }

    dom::Value get(std::size_t i) const override
    {
        if(i < list_.size())
            return dom::create<DomLocation>(list_[i]);
        return nullptr;
    }
};

class DomSourceInfo : public dom::Object
{
public:
    explicit
    DomSourceInfo(
        SourceInfo const& I) noexcept
    {
        if(I.DefLoc)
            append({
                { "def", dom::create<
                    DomLocation>(*I.DefLoc) }});
        if(! I.Loc.empty())
            append({
                { "decl", dom::create<
                    DomLocationArray>(I.Loc) }});
    }
};

//------------------------------------------------
//
// TypeInfo
//
//------------------------------------------------

class DomTypeInfo : public dom::Object
{
public:
    DomTypeInfo(
        TypeInfo const& I,
        Corpus const& corpus)
        : Object({
            { "id", toBase16(I.id) },
            { "name", I.Name },
            { "symbol",
                domCreateInfoOrNull(I.id, corpus) }
            })
    {
    }
};

static
dom::Value
domCreate(
    std::optional<TypeInfo> const& I,
    Corpus const& corpus)
{
    if(I)
        return dom::create<DomTypeInfo>(*I, corpus);
    return nullptr;
}

//------------------------------------------------
//
// Param
//
//------------------------------------------------

/** A function parameter
*/
class DomParam : public dom::Object
{
public:
    DomParam(
        Param const& I,
        Corpus const& corpus) noexcept
        : Object({
            {"name", dom::stringOrNull(I.Name)},
            {"type", dom::create<DomTypeInfo>(I.Type, corpus)},
            {"default", dom::stringOrNull(I.Default)}
        })
    {
    }
};

/** An array of function parameters
*/
class DomParamArray : public dom::Array
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

    std::size_t length() const noexcept override
    {
        return list_.size();
    }

    dom::Value get(std::size_t index) const override
    {
        if(index < list_.size())
            return dom::create<DomParam>(
                list_[index], corpus_);
        return nullptr;
    }
};

//------------------------------------------------
//
// TemplateInfo
//
//------------------------------------------------

class DomTParam : public dom::Object
{
public:
     DomTParam(
        TParam const& I,
        Corpus const& corpus);
};

class DomTArg : public dom::Object
{
public:
    DomTArg(
        TArg const& I,
        Corpus const& corpus)
        : Object({
            {"value", dom::stringOrNull(I.Value)}
        })
    {
    }
};

/** An array of template parameters
*/
class DomTParamArray : public dom::Array
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

    std::size_t length() const noexcept override
    {
        return list_.size();
    }

    dom::Value get(std::size_t index) const override
    {
        if(index >= list_.size())
            return nullptr;
        return dom::create<DomTParam>(
            list_[index], corpus_);
    }
};

/** An array of template arguments
*/
class DomTArgArray : public dom::Array
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

    std::size_t length() const noexcept override
    {
        return list_.size();
    }

    dom::Value get(std::size_t index) const override
    {
        if(index >= list_.size())
            return nullptr;
        return dom::create<DomTArg>(
            list_[index], corpus_);
    }
};

class DomTemplate : public dom::Object
{
public:
    DomTemplate(
        TemplateInfo const& I,
        Corpus const& corpus)
        : Object({
            {"kind", toString(I.specializationKind())},
            {"primary", domCreateInfoOrNull(
                *I.Primary, corpus) },
            {"params", dom::create<DomTParamArray>(
                I.Params, corpus)},
            {"args", dom::create<DomTArgArray>(
                I.Args, corpus)}
        })
    {
    }
};

static
dom::Value
getTParamDefault(
    TParam const& I,
    Corpus const& corpus)
{
    switch(I.Kind)
    {
    case TParamKind::Type:
    {
        const auto& P = I.get<TypeTParam>();
        if(! P.Default)
            return nullptr;
        return dom::create<DomTypeInfo>(
            *P.Default, corpus);
    }
    case TParamKind::NonType:
    {
        const auto& P = I.get<NonTypeTParam>();
        if(! P.Default)
            return nullptr;
        return *P.Default;
    }
    case TParamKind::Template:
    {
        const auto& P = I.get<TemplateTParam>();
        if(! P.Default)
            return nullptr;
        return *P.Default;
    }
    default:
        MRDOX_UNREACHABLE();
    }
}

// this is here for circular references
DomTParam::
DomTParam(
    TParam const& I,
    Corpus const& corpus)
    : Object({
        {"kind", toString(I.Kind)},
        {"name", dom::stringOrNull(I.Name)},
        {"is-pack", I.IsParameterPack},
        {"type", I.Kind == TParamKind::NonType ?
            dom::create<DomTypeInfo>(
                I.get<NonTypeTParam>().Type, corpus) :
            dom::Value()},
        {"params", I.Kind == TParamKind::Template ?
            dom::create<DomTParamArray>(
                I.get<TemplateTParam>().Params, corpus) :
            dom::Value()},
        {"default", getTParamDefault(I, corpus)}
    })
{
}

static
dom::Value
domCreate(
    std::unique_ptr<TemplateInfo> const& I,
    Corpus const& corpus)
{
    if(I)
        return dom::create<DomTemplate>(*I, corpus);
    return nullptr;
}

//------------------------------------------------
//
// BaseInfo
//
//------------------------------------------------

class DomBaseInfo : public dom::Object
{
public:
    DomBaseInfo(
        BaseInfo const& I,
        Corpus const& corpus)
        : Object({
            { "access", toString(I.Access) },
            { "isVirtual", I.IsVirtual },
            { "type", domCreate(I.Type, corpus) }
            })
    {
    }

};

class DomBaseArray : public dom::Array
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

    std::size_t length() const noexcept override
    {
        return list_.size();
    }

    dom::Value get(std::size_t index) const override
    {
        if(index < list_.size())
            return dom::create<DomBaseInfo>(
                list_[index], corpus_);
        return nullptr;
    }
};

//------------------------------------------------
//
// EnumValueInfo
//
//------------------------------------------------

class DomEnumValue : public dom::Object
{
public:
    explicit
    DomEnumValue(
        EnumValueInfo const& I) noexcept
        : Object({
            { "name", I.Name },
            { "value", I.Value },
            { "expr", I.ValueExpr },
            { "doc", domCreate(I.javadoc) }
            })
    {
    }
};

class DomEnumValueArray : public dom::Array
{
    std::vector<EnumValueInfo> const& list_;

public:
    explicit
    DomEnumValueArray(
        std::vector<EnumValueInfo> const& list) noexcept
        : list_(list)
    {
    }

    std::size_t length() const noexcept override
    {
        return list_.size();
    }

    dom::Value get(std::size_t i) const override
    {
        if(i < list_.size())
            return dom::create<DomEnumValue>(list_[i]);
        return nullptr;
    }
};

//------------------------------------------------
//
// Interface
//
//------------------------------------------------

template<class T>
class DomTrancheArray : public dom::Array
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

    std::size_t length() const noexcept override
    {
        return list_.size();
    }

    dom::Value
    get(std::size_t index) const override
    {
        if(index < list_.size())
            return domEagerCreateInfo(
                *list_[index], sp_->corpus);
        return nullptr;
    }
};

class DomTranche : public dom::Object
{
    std::shared_ptr<Interface> sp_;
    Interface::Tranche const& tranche_;

    template<class T>
    static
    dom::Value
    domCreateTrancheArray(
        std::span<T const*> list,
        std::shared_ptr<Interface> const& sp)
    {
        return dom::create<DomTrancheArray<T>>(list, sp);
    }

public:
    DomTranche(
        Interface::Tranche const& tranche,
        std::shared_ptr<Interface> const& sp) noexcept
        : dom::Object({
            { "records",    domCreateTrancheArray(tranche.Records, sp) },
            { "functions",  domCreateTrancheArray(tranche.Functions, sp) },
            { "enums",      domCreateTrancheArray(tranche.Enums, sp) },
            { "types",      domCreateTrancheArray(tranche.Types, sp) },
            { "field",      domCreateTrancheArray(tranche.Data, sp) },
            { "staticfuncs",domCreateTrancheArray(tranche.StaticFunctions, sp) },
            { "staticdata", domCreateTrancheArray(tranche.StaticData, sp) }
            })
        , sp_(sp)
        , tranche_(tranche)
    {
    }
};

class DomInterface : public dom::LazyObject
{
    RecordInfo const& I_;
    std::atomic<std::shared_ptr<
        Interface>> mutable sp_;
    Corpus const& corpus_;

public:
    DomInterface(
        RecordInfo const& I,
        Corpus const& corpus)
        : I_(I)
        , corpus_(corpus)
    {
    }

    std::shared_ptr<Interface>
    getInterface() const
    {
        if(auto sp = sp_.load())
            return sp;
        // If there is a data race, there might
        // be one or more superfluous constructions.
        auto sp = makeInterface(I_, corpus_);
        std::shared_ptr<Interface> expected;
        if(sp_.compare_exchange_strong(expected, sp))
            return sp;
        return expected;
    }

    dom::ObjectPtr construct() const override
    {
        return dom::create<dom::Object>(
            dom::Object::list_type({
                { "public",
                    dom::create<DomTranche>(
                        getInterface()->Public,
                        getInterface()) },
                { "protected",
                    dom::create<DomTranche>(
                        getInterface()->Protected,
                        getInterface()) },
                { "private",
                    dom::create<DomTranche>(
                        getInterface()->Private,
                        getInterface()) },
                }));
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
class DomInfo : public dom::LazyObject
{
    T const& I_;
    Corpus const& corpus_;

    static_assert(std::derived_from<T, Info>);

public:
    DomInfo(
        T const& I,
        Corpus const& corpus) noexcept
        : I_(I)
        , corpus_(corpus)
    {
    }

    dom::ObjectPtr construct() const override;
};

template<class T>
dom::ObjectPtr
DomInfo<T>::
construct() const
{
    dom::Object::list_type list;
    list.insert(list.end(), {
        { "id", toBase16(I_.id) },
        { "kind", toString(I_.Kind) },
        { "access", toString(I_.Access) },
        { "name", I_.Name },
        { "namespace", dom::create<DomSymbolArray>(
            I_.Namespace, corpus_) },
        { "doc", domCreate(I_.javadoc) }
        });
    if constexpr(std::derived_from<T, SourceInfo>)
    {
        list.emplace_back(
            "loc", dom::create<DomSourceInfo>(I_));
    }
    if constexpr(T::isNamespace())
    {
        list.insert(list.end(), {
            { "members", dom::create<DomSymbolArray>(
                I_.Members, corpus_) },
            { "specializations", nullptr }
            });
    }
    if constexpr(T::isRecord())
    {
        list.insert(list.end(), {
            { "tag", toString(I_.KeyKind) },
            { "defaultAccess", getDefaultAccess(I_) },
            { "isTypedef", I_.IsTypeDef },
            { "bases", dom::create<DomBaseArray>(I_.Bases, corpus_) },
            { "friends",  dom::create<DomSymbolArray>(
                I_.Friends, corpus_) },
            { "members", dom::create<DomSymbolArray>(
                I_.Members, corpus_) },
            { "specializations", dom::create<DomSymbolArray>(
                I_.Specializations, corpus_) },
            { "interface", dom::create<DomInterface>(I_, corpus_) },
            { "template", domCreate(I_.Template, corpus_) }
            });
    }
    if constexpr(T::isFunction())
    {
        list.insert(list.end(), {
            { "params",     dom::create<DomParamArray>(I_.Params, corpus_) },
            { "return",     dom::create<DomTypeInfo>(I_.ReturnType, corpus_) },
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
        list.insert(list.end(), {
            { "type", domCreate(I_.BaseType, corpus_) },
            { "members", dom::create<DomEnumValueArray>(I_.Members) },
            { "isScoped", I_.Scoped }
            });
    }
    if constexpr(T::isTypedef())
    {
        list.insert(list.end(), {
            { "template", domCreate(I_.Template, corpus_) }
            });
    }
    if constexpr(T::isVariable())
    {
        list.insert(list.end(), {
            { "type", dom::create<DomTypeInfo>(I_.Type, corpus_) },
            { "template", domCreate(I_.Template, corpus_) },
            { "storageClass", toString(I_.specs.storageClass) }
            });
    }
    if constexpr(T::isField())
    {
        list.insert(list.end(), {
            { "type", dom::create<DomTypeInfo>(I_.Type, corpus_) },
            { "default", dom::stringOrNull(I_.Default) },
            { "isNodiscard", I_.specs.isNodiscard.get() },
            { "isDeprecated", I_.specs.isDeprecated.get() },
            { "hasNoUniqueAddress", I_.specs.hasNoUniqueAddress.get() }
            });
    }
    if constexpr(T::isSpecialization())
    {
    }
    return dom::create<dom::Object>(std::move(list));
}

//------------------------------------------------

} // (anon)

#if 0
/** Return a Dom node for the given metadata.
*/
MRDOX_DECL
dom::Value
domCreateInfo(
    Info const& I,
    Corpus const& corpus);

// Return an Info node, or null if the id is empty
inline
dom::Value
domCreateInfoOrNull(
    OptionalSymbolID const& id,
    Corpus const& corpus)
{
    if(id)
        return domCreateInfo(*id, corpus);
    return nullptr;
}
#endif

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
            return dom::create<DomInfo<T>>(I, corpus);
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
dom::Value
domCreateInfo(
    SymbolID const& id,
    Corpus const& corpus)
{
    // VFALCO Hack to deal with symbol IDs
    // being emitted without the corresponding data.
    auto I = corpus.find(id);
    if(I)
        return domCreateInfo(*I, corpus);
    return nullptr;
}

} // mrdox
} // clang
