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
#include <mrdox/Dom/DomBase.hpp>
#include <mrdox/Dom/DomBaseArray.hpp>
#include <mrdox/Dom/DomFnSpecs.hpp>
#include <mrdox/Dom/DomFnSpecs.hpp>
#include <mrdox/Dom/DomInterface.hpp>
#include <mrdox/Dom/DomJavadoc.hpp>
#include <mrdox/Dom/DomLocation.hpp>
#include <mrdox/Dom/DomParamArray.hpp>
#include <mrdox/Dom/DomSource.hpp>
#include <mrdox/Dom/DomSymbol.hpp>
#include <mrdox/Dom/DomSymbolArray.hpp>
#include <mrdox/Dom/DomTemplate.hpp>
#include <mrdox/Dom/DomType.hpp>
#include <mrdox/Metadata/Interface.hpp>

namespace clang {
namespace mrdox {

template<class T>
DomSymbol<T>::
DomSymbol(
    T const& I,
    Corpus const& corpus) noexcept
    : I_(I)
    , corpus_(corpus)
{
}

template<class T>
dom::Value
DomSymbol<T>::
get(std::string_view key) const
{
    if(key == "id")
        return toBase16(I_.id);
    if(key == "kind")
        return toString(I_.Kind);
    if(key == "access")
        return toString(I_.Access);
    if(key == "name")
        return I_.Name;
    if(key == "namespace")
        return dom::create<DomSymbolArray>(
            I_.Namespace, corpus_);
    if(key == "doc")
    {
        if(I_.javadoc)
            return dom::create<DomJavadoc>(
                *I_.javadoc, corpus_);
        return nullptr;
    }
    if constexpr(std::derived_from<T, SourceInfo>)
    {
        if(key == "loc")
            return dom::create<DomSource>(I_, corpus_);
    }
    if constexpr(T::isNamespace())
    {
        if(key == "members")
            return dom::create<DomSymbolArray>(
                I_.Members, corpus_);
        if(key == "specializations")
            return nullptr;
    }
    if constexpr(T::isRecord())
    {
        if(key == "tag")
            return toString(I_.KeyKind);
        if(key == "defaultAccess")
        {
            switch(I_.KeyKind)
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
        if(key == "isTypedef")
            return I_.IsTypeDef;
        if(key == "bases")
            return dom::create<DomBaseArray>(
                I_.Bases, corpus_);
        if(key == "friends")
            return dom::create<DomSymbolArray>(
                I_.Friends, corpus_);
        if(key == "members")
            return dom::create<DomSymbolArray>(
                I_.Members, corpus_);
        if(key == "specializations")
            return dom::create<DomSymbolArray>(
                I_.Specializations, corpus_);
        if(key == "template")
        {
            if(! I_.Template)
                return nullptr;
            return dom::create<DomTemplate>(
                *I_.Template, corpus_);
        }
        if(key == "interface")
            return dom::create<DomInterface>(
                makeInterface(I_, corpus_), corpus_);
    }
    if constexpr(T::isFunction())
    {
        if(key == "params")
            return dom::create<DomParamArray>(
                I_.Params, corpus_);
        if(key == "return")
            return dom::create<DomType>(
                I_.ReturnType, corpus_);
        if(key == "specs")
            return dom::create<DomFnSpecs>(I_, corpus_);
        if(key == "template")
        {
            if(! I_.Template)
                return nullptr;
            return dom::create<DomTemplate>(
                *I_.Template, corpus_);
        }
    }
    if constexpr(T::isEnum())
    {
    }
    if constexpr(T::isTypedef())
    {
        if(key == "template")
        {
            if(! I_.Template)
                return nullptr;
            return dom::create<DomTemplate>(*I_.Template, corpus_);
        }
    }
    if constexpr(T::isVariable())
    {
        if(key == "template")
        {
            if(! I_.Template)
                return nullptr;
            return dom::create<DomTemplate>(*I_.Template, corpus_);
        }
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
            "defaultAccess",
            "interface",
            "isTypedef",
            "bases",
            "friends",
            "members",
            "specializations",
            "template"
            });
    if constexpr(T::isFunction())
        v.insert(v.end(), {
            "return",
            "params",
            "specs",
            "template"
            });
    if constexpr(T::isVariable())
        v.insert(v.end(), {
            "template"
            });
    if constexpr(T::isTypedef())
        v.insert(v.end(), {
            "template"
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
