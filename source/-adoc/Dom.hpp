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

#include <mrdox/Metadata.hpp>
#include <mrdox/Support/Dom.hpp>

namespace clang {
namespace mrdox {
namespace adoc {

//------------------------------------------------

/** A vector of symbol IDs.
*/
class Symbols : public dom::Array::Impl
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
class Doc : public dom::Object::Impl
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
        return nullptr;
    }
};

//------------------------------------------------

/** A Location
*/
class Loc : public dom::Object::Impl
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
};

//------------------------------------------------

/** Any Info-derived type.
*/
template<class T>
requires std::derived_from<T, Info>
class Symbol : public dom::Object::Impl
{
    static_assert(! std::is_same_v<T, Info>);

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
    get(std::string_view key) const override
    {
        if(key == "kind")
            return I_->symbolType();
        if(key == "id")
            return toBase16(I_->id);
        if(key == "access")
            return toString(I_->Access);
        if(key == "namespace")
            return dom::makeArray<Symbols>(
                I_->Namespace, corpus_);
        if(key == "doc")
        {
            if(I_->javadoc)
                return dom::makeObject<Doc>(
                    *I_->javadoc, corpus_);
            return nullptr;
        }
        if constexpr(std::derived_from<T, SourceInfo>)
        {
            if(key == "definition")
            {
                if(I_->DefLoc.has_value())
                    return dom::makeObject<Loc>(
                        *I_->DefLoc, corpus_);
                return nullptr;
            }
            if(key == "references")
            {
                // VFALCO TODO array of Loc
                return nullptr;
            }
        }
        if constexpr(T::isNamespace())
        {
            if(key == "members")
                return dom::makeArray<Symbols>(
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
            if(key == "friends")
                return dom::makeArray<Symbols>(
                    I_->Friends, corpus_);
            if(key == "members")
                return dom::makeArray<Symbols>(
                    I_->Members, corpus_);
            if(key == "specializations")
                return dom::makeArray<Symbols>(
                    I_->Specializations, corpus_);
        }
        if constexpr(T::isFunction())
        {
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
        return {};
    }
};

//------------------------------------------------

/** Any Info-derived type.
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
};

//------------------------------------------------

/** A vector of BaseInfo.
*/
class Bases : public dom::Array::Impl
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
            return dom::makeObject<Base>(
                corpus_.get<RecordInfo>(
                    list_[index].id),
                list_[index],
                corpus_);
        return nullptr;
    }
};

//------------------------------------------------

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
                return dom::makeObject<
                    Symbol<T>>(I, corpus_);
            });
    return nullptr;
}

} // adoc
} // mrdox
} // clang

#endif
