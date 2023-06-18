//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_LIB_ADOC_DOM_HPP
#define MRDOX_LIB_ADOC_DOM_HPP

#include "DocVisitor.hpp"
#include <mrdox/Metadata.hpp>
#include <mrdox/Support/Dom.hpp>
#include <atomic>

namespace clang {
namespace mrdox {
namespace adoc {

//------------------------------------------------

/** A vector of symbol IDs.
*/
class Symbols : public dom::Array
{
    std::vector<SymbolID> const& list_;
    Corpus const& corpus_;

public:
    Symbols(
        std::vector<SymbolID> const& list,
        Corpus const& corpus) noexcept
        : list_(list)
        , corpus_(corpus)
    {
    }

    std::size_t
    length() const noexcept override
    {
        return list_.size();
    }

    dom::Value
    get(std::size_t index) const override;
};

//------------------------------------------------

/** A Javadoc
*/
class Doc : public dom::Object
{
    Javadoc const* jd_;
    Corpus const& corpus_;

public:
    explicit
    Doc(Javadoc const& jd,
        Corpus const& corpus) noexcept
        : jd_(&jd)
        , corpus_(corpus)
    {
    }

    dom::Value
    get(std::string_view key) const override
    {
        if(key == "brief")
        {
            std::string s;
            DocVisitor visitor(s);
            s.clear();
            if(auto brief = jd_->getBrief())
                visitor(*brief);
            return dom::nonEmptyString(s);
        }
        if(key == "description")
        {
            if(jd_->getBlocks().empty())
                return nullptr;
            std::string s;
            DocVisitor visitor(s);
            visitor(jd_->getBlocks());
            return dom::nonEmptyString(s);
        }
        return nullptr;
    }

    std::vector<std::string_view>
    props() const override
    {
        return {
            "brief",
            "description"
        };
    }
};

//------------------------------------------------

/** A Location
*/
class Loc : public dom::Object
{
    Location const* loc_;
    Corpus const& corpus_;

public:
    Loc(
        Location const& loc,
        Corpus const& corpus) noexcept
        : loc_(&loc)
        , corpus_(corpus)
    {
        (void)corpus_;
    }

    dom::Value
    get(std::string_view key) const override
    {
        if(key == "file")
            return loc_->Filename;
        if(key == "line")
            return loc_->LineNumber;
        return nullptr;
    }

    std::vector<std::string_view>
    props() const override
    {
        return {
            "file",
            "line"
        };
    }
};

//------------------------------------------------

/** A type
*/
class Type : public dom::Object
{
    mrdox::TypeInfo const* I_;
    mrdox::Info const* J_;
    Corpus const& corpus_;

public:
    Type(
        mrdox::TypeInfo const& I,
        Corpus const& corpus) noexcept
        : I_(&I)
        , corpus_(corpus)
    {
        if(I_->id != SymbolID::zero)
            J_ = corpus_.find(I_->id);
        else
            J_ = nullptr;
    }

    dom::Value get(std::string_view key) const override;

    std::vector<std::string_view>
    props() const override
    {
        return { "id", "name", "symbol" };
    }
};

/** A function parameter
*/
class Param : public dom::Object
{
    mrdox::Param const* I_;
    Corpus const& corpus_;

public:
    Param(
        mrdox::Param const& I,
        Corpus const& corpus) noexcept
        : I_(&I)
        , corpus_(corpus)
    {
    }

    dom::Value
    get(std::string_view key) const override
    {
        if(key == "name")
            return dom::nonEmptyString(I_->Name);
        if(key == "type")
            return dom::makePointer<Type>(I_->Type, corpus_);
        if(key == "default")
            return dom::nonEmptyString(I_->Default);
        return nullptr;
    }

    std::vector<std::string_view>
    props() const override
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
class Params : public dom::Array
{
    std::vector<mrdox::Param> const& list_;
    Corpus const& corpus_;

public:
    Params(
        std::vector<mrdox::Param> const& list,
        Corpus const& corpus) noexcept
        : list_(list)
        , corpus_(corpus)
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
            return dom::makePointer<Param>(
                list_[index], corpus_);
        return nullptr;
    }
};

//------------------------------------------------

/** Any Info-derived type.
*/
template<class T>
class Symbol : public dom::Object
{
    static_assert(! std::is_same_v<T, Info>);
    static_assert(std::derived_from<T, Info>);

protected:
    T const* I_;
    Corpus const& corpus_;

public:
    Symbol(
        T const& I,
        Corpus const& corpus) noexcept
        : I_(&I)
        , corpus_(corpus)
    {
    }

    dom::Value
    get(std::string_view key) const override;

    std::vector<std::string_view>
    props() const override
    {
        std::vector<std::string_view> v;
        v.insert(v.end(), {
            "id",
            "kind",
            "name",
            "access",
            "namespace",
            "doc"
            });
        if constexpr(std::derived_from<T, SourceInfo>)
            v.insert(v.end(), {
                "loc"
                });
        if constexpr(T::isNamespace())
            v.insert(v.end(), {
                "members",
                "specializations",
                });
        if constexpr(T::isRecord())
            v.insert(v.end(), {
                "tag",
                "is-typedef",
                "bases",
                "friends",
                "members",
                "specializations"
                });
        if constexpr(T::isFunction())
            v.insert(v.end(), {
                "return",
                "params"
                });
        return v;
    }
};

//------------------------------------------------

/** A base class record
*/
class Base : public Symbol<RecordInfo>
{
    BaseInfo const* B_;

public:
    Base(
        RecordInfo const& I,
        BaseInfo const& B,
        Corpus const& corpus) noexcept
        : Symbol<RecordInfo>(I, corpus)
        , B_(&B)
    {
    }

    dom::Value
    get(std::string_view key) const override
    {
        if(key == "name")
        {
            // this overrides the name in the Info,
            // for the case where the symbolID is zero.
            if(B_->id != SymbolID::zero)
                return I_->Name;
            return B_->Name;
        }
        if(key == "base-access")
            return toString(B_->Access);
        if(key == "is-virtual")
            return B_->IsVirtual;

        return Symbol<RecordInfo>::get(key);
    }

    std::vector<std::string_view>
    props() const override
    {
        std::vector<std::string_view> v =
            Symbol<RecordInfo>::props();
        v.insert(v.end(), {
            //"name", // already there from Symbol
            "base-access",
            "is-virtual"
            });
        return v;
    }
};

//------------------------------------------------

/** A vector of BaseInfo.
*/
class Bases : public dom::Array
{
    std::vector<BaseInfo> const& list_;
    Corpus const& corpus_;

public:
    Bases(
        std::vector<BaseInfo> const& list,
        Corpus const& corpus) noexcept
        : list_(list)
        , corpus_(corpus)
    {
    }

    std::size_t
    length() const noexcept override
    {
        return list_.size();
    }

    dom::Value
    get(
        std::size_t index) const override
    {
        if(index < list_.size())
            return dom::makePointer<Base>(
                corpus_.get<RecordInfo>(
                    list_[index].id),
                list_[index],
                corpus_);
        return nullptr;
    }
};

//------------------------------------------------

template<class T>
dom::Value
Symbol<T>::
get(std::string_view key) const
{
    if(key == "id")
        return toBase16(I_->id);
    if(key == "kind")
        return I_->symbolType();
    if(key == "access")
        return toString(I_->Access);
    if(key == "name")
        return I_->Name;
    if(key == "namespace")
        return dom::makePointer<Symbols>(
            I_->Namespace, corpus_);
    if(key == "doc")
    {
        if(I_->javadoc)
            return dom::makePointer<Doc>(
                *I_->javadoc, corpus_);
        return nullptr;
    }
    if constexpr(std::derived_from<T, SourceInfo>)
    {
        if(key == "loc")
        {
            if(I_->DefLoc.has_value())
                return dom::makePointer<Loc>(
                    *I_->DefLoc, corpus_);
            return nullptr;
        }
    }
    if constexpr(T::isNamespace())
    {
        if(key == "members")
            return dom::makePointer<Symbols>(
                I_->Members, corpus_);
        if(key == "specializations")
            return nullptr;
    }
    if constexpr(T::isRecord())
    {
        if(key == "tag")
        {
            switch(I_->KeyKind)
            {
            case RecordKeyKind::Class:  return "class";
            case RecordKeyKind::Struct: return "struct";
            case RecordKeyKind::Union:  return "union";
            default:
                MRDOX_UNREACHABLE();
            }
        }
        if(key == "is-typedef")
            return I_->IsTypeDef;
        if(key == "bases")
            return dom::makePointer<Bases>(
                I_->Bases, corpus_);
        if(key == "friends")
            return dom::makePointer<Symbols>(
                I_->Friends, corpus_);
        if(key == "members")
            return dom::makePointer<Symbols>(
                I_->Members, corpus_);
        if(key == "specializations")
            return dom::makePointer<Symbols>(
                I_->Specializations, corpus_);
    }
    if constexpr(T::isFunction())
    {
        if(key == "params")
            return dom::makePointer<Params>(
                I_->Params, corpus_);
        if(key == "return")
            return dom::makePointer<Type>(
                I_->ReturnType, corpus_);
    }
    if constexpr(T::isEnum())
    {
    }
    if constexpr(T::isTypedef())
    {
    }
    if constexpr(T::isVariable())
    {
    }
    if constexpr(T::isField())
    {
    }
    if constexpr(T::isSpecialization())
    {
    }
    return nullptr;
}

dom::Value
Type::
get(std::string_view key) const
{
    if(key == "id")
        return toBase16(I_->id);
    if(key == "name")
    {
        if(J_)
            return dom::nonEmptyString(J_->Name);
        return dom::nonEmptyString(I_->Name);
    }
    if(key == "symbol")
    {
        if(! J_)
            return nullptr;
        return visit(
            *J_,
            [&]<class T>(T const& I)
            {
                return dom::Pointer<dom::Object>(
                    dom::makePointer<Symbol<T>>(I, corpus_));
            });
    }
    return nullptr;
}

dom::Value
Symbols::
get(
    std::size_t index) const
{
    if(index < list_.size())
        return visit(
            corpus_.get(list_[index]),
            [&]<class T>(T const& I)
            {
                return dom::Pointer<dom::Object>(
                    dom::makePointer<Symbol<T>>(I, corpus_));
            });
    return nullptr;
}

} // adoc
} // mrdox
} // clang

#endif
