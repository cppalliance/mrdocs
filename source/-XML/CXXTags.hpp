//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_XML_CXXTAGS_HPP
#define MRDOX_XML_CXXTAGS_HPP

#include "XMLTags.hpp"
#include "Support/Debug.hpp"
#include "Support/Operator.hpp"
#include <clang/AST/Attr.h>
#include <mrdox/Metadata/Function.hpp>
#include <mrdox/Metadata/Record.hpp>
#include <mrdox/Metadata/Type.hpp>
#include <mrdox/Metadata/Var.hpp>

/*
    This file holds the business logic for transforming
    metadata into XML tags. Constants here are reflected
    in the MRDOX DTD XML schema.
*/

namespace clang {
namespace mrdox {
namespace xml {

constexpr llvm::StringRef accessTagName     = "access";
constexpr llvm::StringRef aliasTagName      = "alias";
constexpr llvm::StringRef attributeTagName  = "attr";
constexpr llvm::StringRef baseTagName       = "base";
constexpr llvm::StringRef classTagName      = "class";
constexpr llvm::StringRef dataMemberTagName = "data";
constexpr llvm::StringRef javadocTagName    = "doc";
constexpr llvm::StringRef enumTagName       = "enum";
constexpr llvm::StringRef friendTagName     = "friend";
constexpr llvm::StringRef functionTagName   = "function";
constexpr llvm::StringRef interfaceTagName  = "interface";
constexpr llvm::StringRef namespaceTagName  = "namespace";
constexpr llvm::StringRef paramTagName      = "param";
constexpr llvm::StringRef returnTagName     = "return";
constexpr llvm::StringRef structTagName     = "struct";
constexpr llvm::StringRef targTagName       = "targ";
constexpr llvm::StringRef templateTagName   = "template";
constexpr llvm::StringRef tparamTagName     = "tparam";
constexpr llvm::StringRef typedefTagName    = "typedef";
constexpr llvm::StringRef unionTagName      = "union";
constexpr llvm::StringRef varTagName        = "variable";

constexpr llvm::StringRef getNameForValue(...)
{
    return "";
}

constexpr llvm::StringRef getNameForValue(ConstexprSpecKind CSK)
{
    switch(CSK)
    {
    case ConstexprSpecKind::Constexpr: return "constexpr";
    case ConstexprSpecKind::Consteval: return "consteval";
    case ConstexprSpecKind::Constinit: return "constinit";
    default:
        Assert(!"Invalid ConstexprSpecKind");
    }
    return "";
}

constexpr llvm::StringRef getNameForValue(ExceptionSpecificationType EST)
{
    switch(EST)
    {
    case ExceptionSpecificationType::EST_DynamicNone:       return "throw";
    case ExceptionSpecificationType::EST_Dynamic:           return "throw-expr";
    case ExceptionSpecificationType::EST_MSAny:             return "ms-throw";
    case ExceptionSpecificationType::EST_NoThrow:           return "ms-nothrow";
    case ExceptionSpecificationType::EST_BasicNoexcept:     return "noexcept";
    case ExceptionSpecificationType::EST_DependentNoexcept: return "noexcept-expr";
    case ExceptionSpecificationType::EST_NoexceptFalse:     return "noexcept-false";
    case ExceptionSpecificationType::EST_NoexceptTrue:      return "noexcept-true";
    default:
        Assert(false);
    }
    return "";
}

inline llvm::StringRef getNameForValue(OverloadedOperatorKind OOK)
{
    return getSafeOperatorName(OOK);
}

constexpr llvm::StringRef getNameForValue(StorageClass SC)
{
    switch(SC)
    {
    case StorageClass::SC_Extern:        return "extern";
    case StorageClass::SC_Static:        return "static";
    case StorageClass::SC_PrivateExtern: return "extern-private";
    case StorageClass::SC_Auto:          return "auto";
    case StorageClass::SC_Register:      return "register";
    default:
        Assert(false);
    }
    return "";
}

constexpr llvm::StringRef getNameForValue(WarnUnusedResultAttr::Spelling WUS)
{
    switch(WUS)
    {
    case WarnUnusedResultAttr::Spelling::CXX11_nodiscard:              return "nodiscard";
    case WarnUnusedResultAttr::Spelling::C2x_nodiscard:                return "nodiscard-C2x";
    case WarnUnusedResultAttr::Spelling::GNU_warn_unused_result:       return "gnu-warn-unused";
    case WarnUnusedResultAttr::Spelling::CXX11_gnu_warn_unused_result: return "gnu-warn-unused-cxx11";
    case WarnUnusedResultAttr::Spelling::C2x_gnu_warn_unused_result:   return "gnu-warn-unused-C2x";
    default:
        Assert(false);
    }
    return "";
}

constexpr llvm::StringRef getNameForValue(RefQualifierKind RK)
{
    switch(RK)
    {
    case RefQualifierKind::RQ_LValue: return "lv";
    case RefQualifierKind::RQ_RValue: return "rv";
    default:
        Assert(false);
    }
    return "";
}

//------------------------------------------------

template<class BitFieldUnion>
struct BitFieldWriter
{
    BitFieldUnion field;
    XMLTags& tags;

    BitFieldWriter(BitFieldUnion field, XMLTags& tags)
        : field(field), tags(tags)
    {
    }

    template<unsigned char Offset,
             unsigned char Size,
             typename T>
    void write(BitField<Offset, Size, T> BitFieldUnion :: * member,
                          const char * idName)
    {
        const auto v = (field.*member).get();

        if constexpr (std::is_enum_v<T>)
        {
            if(static_cast<std::uint32_t>(v) == 0)
                return;
            tags.write(attributeTagName, {}, {
                    { "id", idName },
                    { "name", getNameForValue(v) },
                    { "value", std::to_string(static_cast<
                                      std::underlying_type_t<T>>(v)) } });
        }
        else if constexpr (BitField<Offset, Size, T>::size == 1u)
        {
            if (v)
                tags.write(attributeTagName, {}, { { "id", idName } });
        }
        else
            tags.write(attributeTagName, {}, {
                    { "id", idName },
                    { "value", std::to_string(v) } });
    }
};

inline void write(RecFlags0 const& bits, XMLTags& tags)
{
    BitFieldWriter<RecFlags0> fw(bits, tags);
    fw.write(&RecFlags0::isFinal, "is-final");
    fw.write(&RecFlags0::isFinalDestructor, "is-final-dtor");
}

inline void write(FnFlags0 const& bits, XMLTags& tags)
{
    BitFieldWriter<FnFlags0> fw(bits, tags);
    fw.write(&FnFlags0::isVariadic,            "is-variadic");
    fw.write(&FnFlags0::isVirtualAsWritten,    "is-virtual-as-written");
    fw.write(&FnFlags0::isPure,                "is-pure");
    fw.write(&FnFlags0::isDefaulted,           "is-defaulted");
    fw.write(&FnFlags0::isExplicitlyDefaulted, "is-explicitly-defaulted");
    fw.write(&FnFlags0::isDeleted,             "is-deleted");
    fw.write(&FnFlags0::isDeletedAsWritten,    "is-deleted-as-written");
    fw.write(&FnFlags0::isNoReturn,            "is-no-return");
    fw.write(&FnFlags0::hasOverrideAttr,       "has-override");
    fw.write(&FnFlags0::hasTrailingReturn,     "has-trailing-return");
    fw.write(&FnFlags0::constexprKind,         "constexpr-kind");
    fw.write(&FnFlags0::exceptionSpecType,     "exception-spec");
    fw.write(&FnFlags0::overloadedOperator,    "operator");
    fw.write(&FnFlags0::storageClass,          "storage-class");
    fw.write(&FnFlags0::isConst,               "is-const");
    fw.write(&FnFlags0::isVolatile,            "is-volatile");
    fw.write(&FnFlags0::refQualifier,          "ref-qualifier");
}

inline void write(FnFlags1 const& bits, XMLTags& tags)
{
    BitFieldWriter<FnFlags1> fw(bits, tags);

    fw.write(&FnFlags1::isNodiscard,       "nodiscard");
    fw.write(&FnFlags1::nodiscardSpelling, "nodiscard-spelling");
    fw.write(&FnFlags1::isExplicit,        "is-explicit");
}


inline void write(FieldFlags const& bits, XMLTags& tags)
{
    BitFieldWriter<FieldFlags> fw(bits, tags);

    // KRYSTIAN FIXME: non-static data members cannot be nodiscard
    fw.write(&FieldFlags::isNodiscard, "nodiscard");
    fw.write(&FieldFlags::isDeprecated, "deprecated");
    fw.write(&FieldFlags::hasNoUniqueAddress, "no-unique-address");
}


inline void write(VarFlags0 const& bits, XMLTags& tags)
{
    BitFieldWriter<VarFlags0> fw(bits, tags);
    fw.write(&VarFlags0::storageClass, "storage-class");
}

inline void writeReturnType(TypeInfo const& I, XMLTags& tags)
{
    if(I.Name == "void")
        return;
    tags.write(returnTagName, {}, {
        { "type", I.Name },
        { I.id }
        });
}

inline void writeParam(Param const& P, XMLTags& tags)
{
    tags.write(paramTagName, {}, {
        { "name", P.Name, ! P.Name.empty() },
        { "type", P.Type.Name },
        { "default", P.Default, ! P.Default.empty() },
        { P.Type.id } });
}

inline void writeTemplateParam(const TParam& I, XMLTags& tags)
{
    switch(I.Kind)
    {
    case TParamKind::Type:
    {
        const auto& t = I.get<TypeTParam>();
        std::string_view default_val;
        if(t.Default)
            default_val = t.Default->Name.str();

        tags.write(tparamTagName, {}, {
            { "name", I.Name, ! I.Name.empty() },
            { "class", "type" },
            { "default", default_val, ! default_val.empty() }
        });
        break;
    }
    case TParamKind::NonType:
    {
        const auto& t = I.get<NonTypeTParam>();
        std::string_view default_val;
        if(t.Default)
            default_val = t.Default.value();

        tags.write(tparamTagName, {}, {
            { "name", I.Name, ! I.Name.empty() },
            { "class", "non-type" },
            { "type", t.Type.Name },
            { "default", default_val, ! default_val.empty() }
        });
        break;
    }
    case TParamKind::Template:
    {
        const auto& t = I.get<TemplateTParam>();
        std::string_view default_val;
        if(t.Default)
            default_val = t.Default.value();
        tags.open(tparamTagName, {
            { "name", I.Name, ! I.Name.empty() },
            { "class", "template" },
            { "default", default_val, ! default_val.empty() }
        });
        for(const auto& P : t.Params)
            writeTemplateParam(P, tags);
        tags.close(tparamTagName);
        break;
    }
    default:
    {
        tags.write(tparamTagName, {}, {
            { "name", I.Name, ! I.Name.empty() }
        });
        break;
    }
    }
}

inline void writeTemplateArg(const TArg& I, XMLTags& tags)
{
    tags.write(targTagName, {}, {
        { "value", I.Value }
    });
}

/** Return the xml tag name for the Info.
*/
llvm::StringRef
getTagName(Info const& I) noexcept;

} // xml
} // mrdox
} // clang

#endif
