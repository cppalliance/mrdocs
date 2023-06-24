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
#include <mrdox/Dom/DomFnSpecs.hpp>
#include <mrdox/Dom/DomMetadata.hpp>

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

    std::size_t length() const noexcept
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

static
dom::Value
domCreate(
    SourceInfo const& I)
{
    return dom::create<DomSourceInfo>(I);
}

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
            { "symbol", I.id != SymbolID::zero
                ? domCreateInfo(I.id, corpus)
                : nullptr }
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
//
//------------------------------------------------

/** A function parameter
*/
class DomParam : public dom::Object
{
    Param const* I_;
    Corpus const& corpus_;

public:
    DomParam(
        Param const& I,
        Corpus const& corpus) noexcept
        : I_(&I)
        , corpus_(corpus)
    {
    }

    dom::Value get(std::string_view key) const override
    {
        if(key == "name")
            return dom::nonEmptyString(I_->Name);
        if(key == "type")
            return dom::create<DomTypeInfo>(I_->Type, corpus_);
        if(key == "default")
            return dom::nonEmptyString(I_->Default);
        return nullptr;
    }

    std::vector<std::string_view> props() const override
    {
        return {
            "name",
            "type",
            "default"
        };
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
        if(index >= list_.size())
            return nullptr;
        return dom::create<DomParam>(
            list_[index], corpus_);
    }
};

//------------------------------------------------
//
// TemplateInfo
//
//------------------------------------------------

class DomTParam : public dom::Object
{
    TParam const* I_;
    Corpus const& corpus_;

public:
    DomTParam(
        TParam const& I,
        Corpus const& corpus) noexcept
        : I_(&I)
        , corpus_(corpus)
    {
    }

    dom::Value get(std::string_view key) const override;

    std::vector<std::string_view> props() const override
    {
        return {
            "kind",
            "name",
            "is-pack",
            "default",
            // only for NonTypeTParam
            "type",
            // only for TemplateTParam
            "params"
        };
    }
};

class DomTArg : public dom::Object
{
    TArg const* I_;
    Corpus const& corpus_;

public:
    DomTArg(
        TArg const& I,
        Corpus const& corpus) noexcept
        : I_(&I)
        , corpus_(corpus)
    {
    }

    dom::Value get(std::string_view key) const override
    {
        if(key == "value")
            return dom::nonEmptyString(I_->Value);
        return nullptr;
    }

    std::vector<std::string_view> props() const override
    {
        return {
            "value"
        };
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

class MRDOX_DECL
    DomTemplate : public dom::Object
{
    TemplateInfo const* I_;
    Info const* Primary_;
    Corpus const& corpus_;

public:
    DomTemplate(
        TemplateInfo const& I,
        Corpus const& corpus) noexcept
        : I_(&I)
        , corpus_(corpus)
    {
        if(I_->Primary)
            Primary_ = corpus_.find(*I_->Primary);
        else
            Primary_ = nullptr;
    }

    dom::Value get(std::string_view key) const override
    {
        if(key == "kind")
        {
            switch(I_->specializationKind())
            {
            case TemplateSpecKind::Primary:
                return "primary";
            case TemplateSpecKind::Explicit:
                return "explicit";
            case TemplateSpecKind::Partial:
                return "partial";
            default:
                MRDOX_UNREACHABLE();
            }
        }
        if(key == "primary")
        {
            if(Primary_)
                return domCreateInfo(*Primary_, corpus_);
            return nullptr;
        }
        if(key == "params")
            return dom::create<DomTParamArray>(
                I_->Params, corpus_);
        if(key == "args")
            return dom::create<DomTArgArray>(
                I_->Args, corpus_);
        return nullptr;
    }

    std::vector<std::string_view> props() const override
    {
        return {
            "params",
            "args",
            "kind",
            "primary"
        };
    }
};

// These are here for circular references

dom::Value
DomTParam::
get(std::string_view key) const
{
    if(key == "kind")
    {
        switch(I_->Kind)
        {
        case TParamKind::Type:
            return "type";
        case TParamKind::NonType:
            return "non-type";
        case TParamKind::Template:
            return "template";
        default:
            MRDOX_UNREACHABLE();
        }
    }
    if(key == "name")
        return dom::nonEmptyString(I_->Name);
    if(key == "is-pack")
        return I_->IsParameterPack;
    if(key == "type")
    {
        if(I_->Kind != TParamKind::NonType)
            return nullptr;
        return dom::create<DomTypeInfo>(
            I_->get<NonTypeTParam>().Type, corpus_);
    }
    if(key == "params")
    {
        if(I_->Kind != TParamKind::Template)
            return nullptr;
        return dom::create<DomTParamArray>(
            I_->get<TemplateTParam>().Params, corpus_);
    }
    if(key == "default")
    {
        switch(I_->Kind)
        {
        case TParamKind::Type:
        {
            const auto& P = I_->get<TypeTParam>();
            if(! P.Default)
                return nullptr;
            return dom::create<DomTypeInfo>(
                *P.Default, corpus_);
        }
        case TParamKind::NonType:
        {
            const auto& P = I_->get<NonTypeTParam>();
            if(! P.Default)
                return nullptr;
            return *P.Default;
        }
        case TParamKind::Template:
        {
            const auto& P = I_->get<TemplateTParam>();
            if(! P.Default)
                return nullptr;
            return *P.Default;
        }
        default:
            MRDOX_UNREACHABLE();
        }
    }
    return nullptr;
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

    std::size_t length() const noexcept
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

    std::size_t
    length() const noexcept override
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
            { "params", dom::create<DomParamArray>(I_.Params, corpus_) },
            { "return", dom::create<DomTypeInfo>(I_.ReturnType, corpus_) },
            { "specs", dom::create<DomFnSpecs>(I_, corpus_) },
            { "template", domCreate(I_.Template, corpus_) }
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
            { "default", dom::nonEmptyString(I_.Default) },
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

/** Return a lazy Info node.
*/
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

/** Return a lazy Info node, or null.
*/
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

/** Create an Info node immediately.

    This invokes the factory directly, without
    going through the lazy wrapper.
*/
template<class T>
dom::Value
domEagerCreateInfo(
    T const& I,
    Corpus const& corpus)
{
    return DomInfo<T>(I, corpus).construct();
}

} // mrdox
} // clang
