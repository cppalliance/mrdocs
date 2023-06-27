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

class DomSymbolArray : public dom::LazyArray
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

    dom::ArrayPtr
    construct() const override
    {
        dom::Array::list_type list;
        list.reserve(list_.size());
        for(auto const& id : list_)
            list.emplace_back(
                domCreateInfo(id, corpus_));
        return makeShared<dom::Array>(std::move(list));
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

        return makeShared<dom::Object>(std::move(list));
    }
};

static
dom::Value
domCreate(
    std::unique_ptr<Javadoc> const& jd)
{
    if(jd)
        return makeShared<DomJavadoc>(*jd);
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

    std::size_t size() const noexcept override
    {
        return list_.size();
    }

    dom::Value get(std::size_t i) const override
    {
        if(i < list_.size())
            return makeShared<DomLocation>(list_[i]);
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
                { "def", makeShared<
                    DomLocation>(*I.DefLoc) }});
        if(! I.Loc.empty())
            append({
                { "decl", makeShared<
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
        Corpus const& corpus);
};

static
dom::Value
domCreate(
    std::unique_ptr<TypeInfo> const& I,
    Corpus const& corpus)
{
    if(I)
        return makeShared<DomTypeInfo>(*I, corpus);
    return nullptr;
}

class DomTypeInfoArray : public dom::Array
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

    dom::Value get(std::size_t index) const override
    {
        if(index < list_.size())
            return domCreate(list_[index], corpus_);
        return nullptr;
    }
};

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
            {"type", domCreate(I.Type, corpus)},
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

    std::size_t size() const noexcept override
    {
        return list_.size();
    }

    dom::Value get(std::size_t index) const override
    {
        if(index < list_.size())
            return makeShared<DomParam>(
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

    std::size_t size() const noexcept override
    {
        return list_.size();
    }

    dom::Value get(std::size_t index) const override
    {
        if(index >= list_.size())
            return nullptr;
        return makeShared<DomTParam>(
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

    std::size_t size() const noexcept override
    {
        return list_.size();
    }

    dom::Value get(std::size_t index) const override
    {
        if(index >= list_.size())
            return nullptr;
        return makeShared<DomTArg>(
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
            {"params", makeShared<DomTParamArray>(
                I.Params, corpus)},
            {"args", makeShared<DomTArgArray>(
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
        return domCreate(P.Default, corpus);
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
            domCreate(I.get<NonTypeTParam>().Type, corpus) :
            dom::Value()},
        {"params", I.Kind == TParamKind::Template ?
            makeShared<DomTParamArray>(
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
        return makeShared<DomTemplate>(*I, corpus);
    return nullptr;
}

//------------------------------------------------

static
dom::Object::list_type
makeTypeProps(
    const TypeInfo& I,
    const Corpus& corpus)
{
    dom::Object::list_type result = {{"kind", toString(I.Kind)}};
    visit(I, [&]<typename T>(const T& t)
    {
        if constexpr(requires { t.Name; })
            result.emplace_back("name",
                t.Name);

        if constexpr(requires { t.id; })
        {
            if(t.id != SymbolID::zero && corpus.find(t.id))
                result.emplace_back("id",
                    toBase16(t.id));
        }

        if constexpr(T::isSpecialization())
            result.emplace_back("template-args",
                makeShared<DomTArgArray>(t.TemplateArgs, corpus));

        if constexpr(requires { t.CVQualifiers; })
            result.emplace_back("cv-qualifiers",
                toString(t.CVQualifiers));

        if constexpr(requires { t.ParentType; })
            result.emplace_back("parent-type",
                domCreate(t.ParentType, corpus));

        if constexpr(requires { t.PointeeType; })
            result.emplace_back("pointee-type",
                domCreate(t.PointeeType, corpus));

        if constexpr(T::isPack())
            result.emplace_back("pattern-type",
                domCreate(t.PatternType, corpus));

        if constexpr(T::isArray())
        {
            result.emplace_back("element-type",
                domCreate(t.ElementType, corpus));
            result.emplace_back("bounds-value",
                t.BoundsValue);
            result.emplace_back("bounds-expr",
                t.BoundsExpr);
        }

        if constexpr(T::isFunction())
        {
            result.emplace_back("return-type",
                domCreate(t.ReturnType, corpus));
            result.emplace_back("param-types",
                makeShared<DomTypeInfoArray>(t.ParamTypes, corpus));
            result.emplace_back("exception-spec",
                toString(t.ExceptionSpec));
            result.emplace_back("ref-qualifier",
                toString(t.RefQualifier));
        }
    });
    return result;
}

DomTypeInfo::
DomTypeInfo(
    TypeInfo const& I,
    Corpus const& corpus)
    : Object(makeTypeProps(I, corpus))
{
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

    std::size_t size() const noexcept override
    {
        return list_.size();
    }

    dom::Value get(std::size_t index) const override
    {
        if(index < list_.size())
            return makeShared<DomBaseInfo>(
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

    std::size_t size() const noexcept override
    {
        return list_.size();
    }

    dom::Value get(std::size_t i) const override
    {
        if(i < list_.size())
            return makeShared<DomEnumValue>(list_[i]);
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
    SharedPtr<Interface> sp_;

public:
    DomTrancheArray(
        std::span<T const*> list,
        SharedPtr<Interface> const& sp)
        : list_(list)
        , sp_(sp)
    {
    }

    std::size_t size() const noexcept override
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
    SharedPtr<Interface> sp_;
    Interface::Tranche const& tranche_;

    template<class T>
    static
    dom::Value
    init(
        std::span<T const*> list,
        SharedPtr<Interface> const& sp)
    {
        return makeShared<DomTrancheArray<T>>(list, sp);
    }

public:
    DomTranche(
        Interface::Tranche const& tranche,
        SharedPtr<Interface> const& sp) noexcept
        : dom::Object({
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

class DomInterface : public dom::LazyObject
{
    RecordInfo const& I_;
    AtomicSharedPtr<Interface> sp_;
    Corpus const& corpus_;

public:
    DomInterface(
        RecordInfo const& I,
        Corpus const& corpus)
        : I_(I)
        , corpus_(corpus)
    {
    }

    SharedPtr<Interface>
    getInterface() const
    {
        return sp_.load(
            [&]
            {
                return makeInterface(I_, corpus_);
            });
    }

    dom::ObjectPtr construct() const override
    {
        return makeShared<dom::Object>(
            dom::Object::list_type({
                { "public",
                    makeShared<DomTranche>(
                        getInterface()->Public,
                        getInterface()) },
                { "protected",
                    makeShared<DomTranche>(
                        getInterface()->Protected,
                        getInterface()) },
                { "private",
                    makeShared<DomTranche>(
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
        { "namespace", makeShared<DomSymbolArray>(
            I_.Namespace, corpus_) },
        { "doc", domCreate(I_.javadoc) }
        });
    if constexpr(std::derived_from<T, SourceInfo>)
    {
        list.emplace_back(
            "loc", makeShared<DomSourceInfo>(I_));
    }
    if constexpr(T::isNamespace())
    {
        list.insert(list.end(), {
            { "members", makeShared<DomSymbolArray>(
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
            { "bases", makeShared<DomBaseArray>(I_.Bases, corpus_) },
            { "friends",  makeShared<DomSymbolArray>(
                I_.Friends, corpus_) },
            { "members", makeShared<DomSymbolArray>(
                I_.Members, corpus_) },
            { "specializations", makeShared<DomSymbolArray>(
                I_.Specializations, corpus_) },
            { "interface", makeShared<DomInterface>(I_, corpus_) },
            { "template", domCreate(I_.Template, corpus_) }
            });
    }
    if constexpr(T::isFunction())
    {
        list.insert(list.end(), {
            { "params",     makeShared<DomParamArray>(I_.Params, corpus_) },
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
        list.insert(list.end(), {
            { "type", domCreate(I_.BaseType, corpus_) },
            { "members", makeShared<DomEnumValueArray>(I_.Members) },
            { "isScoped", I_.Scoped }
            });
    }
    if constexpr(T::isTypedef())
    {
        list.insert(list.end(), {
            { "type", domCreate(I_.Underlying, corpus_) },
            { "template", domCreate(I_.Template, corpus_) },
            { "isUsing", I_.IsUsing }
            });
    }
    if constexpr(T::isVariable())
    {
        list.insert(list.end(), {
            { "type", domCreate(I_.Type, corpus_) },
            { "template", domCreate(I_.Template, corpus_) },
            { "storageClass", toString(I_.specs.storageClass) }
            });
    }
    if constexpr(T::isField())
    {
        list.insert(list.end(), {
            { "type", domCreate(I_.Type, corpus_) },
            { "default", dom::stringOrNull(I_.Default) },
            { "isNodiscard", I_.specs.isNodiscard.get() },
            { "isDeprecated", I_.specs.isDeprecated.get() },
            { "hasNoUniqueAddress", I_.specs.hasNoUniqueAddress.get() }
            });
    }
    if constexpr(T::isSpecialization())
    {
    }
    return makeShared<dom::Object>(std::move(list));
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
            return makeShared<DomInfo<T>>(I, corpus);
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
        //return domCreateInfo(*I, corpus);
        return visit(*I,
            [&corpus]<class T>(T const& I) ->
                dom::Value
            {
                return DomInfo<T>(I, corpus).construct();
            });
    return nullptr;
}

//------------------------------------------------

struct DomCorpus::Impl
{
    struct value_type
    {
        std::weak_ptr<dom::Object> weak;
        std::shared_ptr<dom::Object> strong;
    };

    llvm::StringMap<dom::ObjectPtr> cache_;
};

DomCorpus::
~DomCorpus() = default;

DomCorpus::
DomCorpus(
    Corpus const& corpus)
    : corpus_(corpus)
    , impl_(std::make_unique<Impl>())
{
}

dom::ObjectPtr
DomCorpus::
get(SymbolID const& id)
{
    return makeShared<dom::Object>();
}

} // mrdox
} // clang
