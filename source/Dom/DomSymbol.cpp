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
#include <mrdox/Dom/DomBase.hpp>
#include <mrdox/Dom/DomBaseArray.hpp>
#include <mrdox/Dom/DomFnSpecs.hpp>
#include <mrdox/Dom/DomJavadoc.hpp>
#include <mrdox/Dom/DomLocation.hpp>
#include <mrdox/Dom/DomParamArray.hpp>
#include <mrdox/Dom/DomSource.hpp>
#include <mrdox/Dom/DomSymbol.hpp>
#include <mrdox/Dom/DomSymbolArray.hpp>
#include <mrdox/Dom/DomType.hpp>

namespace clang {
namespace mrdox {

template<class T>
DomSymbol<T>::
DomSymbol(
    T const& I,
    Corpus const& corpus) noexcept
    : I_(&I)
    , corpus_(corpus)
{
}

template<class T>
dom::Value
DomSymbol<T>::
get(std::string_view key) const
{
    if(key == "id")
        return toBase16(I_->id);
    if(key == "kind")
        return toString(I_->Kind);
    if(key == "access")
        return toString(I_->Access);
    if(key == "name")
        return I_->Name;
    if(key == "namespace")
        return dom::makePointer<DomSymbolArray>(
            I_->Namespace, corpus_);
    if(key == "doc")
    {
        if(I_->javadoc)
            return dom::makePointer<DomJavadoc>(
                *I_->javadoc, corpus_);
        return nullptr;
    }
    if constexpr(std::derived_from<T, SourceInfo>)
    {
        if(key == "loc")
            return dom::makePointer<DomSource>(*I_, corpus_);
    }
    if constexpr(T::isNamespace())
    {
        if(key == "members")
            return dom::makePointer<DomSymbolArray>(
                I_->Members, corpus_);
        if(key == "specializations")
            return nullptr;
    }
    if constexpr(T::isRecord())
    {
        if(key == "tag")
            return toString(I_->KeyKind);
        if(key == "is-typedef")
            return I_->IsTypeDef;
        if(key == "bases")
            return dom::makePointer<DomBaseArray>(
                I_->Bases, corpus_);
        if(key == "friends")
            return dom::makePointer<DomSymbolArray>(
                I_->Friends, corpus_);
        if(key == "members")
            return dom::makePointer<DomSymbolArray>(
                I_->Members, corpus_);
        if(key == "specializations")
            return dom::makePointer<DomSymbolArray>(
                I_->Specializations, corpus_);
    }
    if constexpr(T::isFunction())
    {
        if(key == "params")
            return dom::makePointer<DomParamArray>(
                I_->Params, corpus_);
        if(key == "return")
            return dom::makePointer<DomType>(
                I_->ReturnType, corpus_);
        if(key == "specs")
            return dom::makePointer<DomFnSpecs>(
                *I_, corpus_);
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

template<class T>
auto
DomSymbol<T>::
props() const ->
    std::vector<std::string_view>
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
            "params",
            "specs"
            });
    return v;
}

// Explicit instantiations
template class DomSymbol<NamespaceInfo>;
template class DomSymbol<RecordInfo>;
template class DomSymbol<FunctionInfo>;
template class DomSymbol<EnumInfo>;
template class DomSymbol<TypedefInfo>;
template class DomSymbol<VariableInfo>;
template class DomSymbol<FieldInfo>;
template class DomSymbol<SpecializationInfo>;

} // mrdox
} // clang
