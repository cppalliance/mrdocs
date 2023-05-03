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

#ifndef MRDOX_API_XML_CXXTAGS_HPP
#define MRDOX_API_XML_CXXTAGS_HPP

#include "XMLTags.hpp"
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

constexpr llvm::StringRef namespaceTagName  = "namespace";
constexpr llvm::StringRef classTagName      = "class";
constexpr llvm::StringRef structTagName     = "struct";
constexpr llvm::StringRef unionTagName      = "union";
constexpr llvm::StringRef functionTagName   = "function";
constexpr llvm::StringRef typedefTagName    = "typedef";
constexpr llvm::StringRef aliasTagName      = "alias";
constexpr llvm::StringRef enumTagName       = "enum";
constexpr llvm::StringRef varTagName        = "var";
constexpr llvm::StringRef attributeTagName  = "attr";
constexpr llvm::StringRef returnTagName     = "return";
constexpr llvm::StringRef paramTagName      = "param";
constexpr llvm::StringRef friendTagName     = "friend";
constexpr llvm::StringRef tparamTagName     = "tparam";
constexpr llvm::StringRef dataMemberTagName = "data";
constexpr llvm::StringRef javadocTagName    = "doc";

constexpr llvm::StringRef getBitsIDName(RecFlags0 ID)
{
    switch(ID)
    {
    case RecFlags0::isFinal:           return "is-final";
    case RecFlags0::isFinalDestructor: return "is-final-dtor";
    default:
        Assert(false);
    }
    return "";
}

constexpr llvm::StringRef getBitsIDName(FnFlags0 ID)
{
    switch(ID)
    {
    case FnFlags0::isVariadic:            return "is-variadic";
    case FnFlags0::isVirtualAsWritten:    return "is-virtual-as-written";
    case FnFlags0::isPure:                return "is-pure";
    case FnFlags0::isDefaulted:           return "is-defaulted";
    case FnFlags0::isExplicitlyDefaulted: return "is-explicitly-defaulted";
    case FnFlags0::isDeleted:             return "is-deleted";
    case FnFlags0::isDeletedAsWritten:    return "is-deleted-as-written";
    case FnFlags0::isNoReturn:            return "is-no-return";
    case FnFlags0::hasOverrideAttr:       return "has-override";
    case FnFlags0::hasTrailingReturn:     return "has-trailing-return";
    case FnFlags0::constexprKind:         return "constexpr-kind";
    case FnFlags0::exceptionSpecType:     return "exception-spec";
    case FnFlags0::overloadedOperator:    return "operator";
    case FnFlags0::storageClass:          return "storage-class";
    case FnFlags0::isConst:               return "is-const";
    case FnFlags0::isVolatile:            return "is-volatile";
    case FnFlags0::refQualifier:          return "ref-qualifier";
    default:
        Assert(false);
    }
    return "";
}

constexpr llvm::StringRef getBitsIDName(FnFlags1 ID)
{
    switch(ID)
    {
    case FnFlags1::isNodiscard:       return "nodiscard";
    case FnFlags1::nodiscardSpelling: return "nodiscard-spelling";
    case FnFlags1::isExplicit:        return "is-explicit";
    default:
        Assert(false);
    }
    return "";
}

constexpr llvm::StringRef getBitsIDName(VarFlags0 ID)
{
    switch(ID)
    {
    case VarFlags0::storageClass:     return "storage-class";
    default:
        Assert(false);
    }
    return "";
}

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
        Assert(false);
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

template<class Enum>
struct WriteBits
{
    Bits<Enum> bits;

    WriteBits(Bits<Enum> const& bits_)
        : bits(bits_)
    {
    }

    template<Enum ID>
    void write(XMLTags& tags)
    {
        auto const v = bits.template get<ID>();
        if(v == 0)
            return;
        if constexpr(std::has_single_bit(BitsValueType(ID)))
        {
            tags.write(attributeTagName, {}, { { "id", getBitsIDName(ID) } });
        }
        else
        {
            tags.write(attributeTagName, {}, {
                { "id", getBitsIDName(ID) },
                { "value", std::to_string(v) } });
        }
    }

    template<Enum ID, class ValueType>
    requires std::is_enum_v<ValueType>
    void write(XMLTags& tags)
    {
        static_assert(! std::has_single_bit(BitsValueType(ID)));
        auto const v = static_cast<ValueType>(bits.template get<ID>());
        if(static_cast<BitsValueType>(v) == 0)
            return;
        tags.write(attributeTagName, {}, {
            { "id", getBitsIDName(ID) },
            { "name", getNameForValue(v) },
            { "value", std::to_string(static_cast<
                std::underlying_type_t<ValueType>>(v)) } });
    }
};

template<class Enum>
WriteBits(Bits<Enum> const&) -> WriteBits<Enum>;

inline void write(Bits<RecFlags0> const& bits, XMLTags& tags)
{
    WriteBits(bits).write<RecFlags0::isFinal>(tags);
    WriteBits(bits).write<RecFlags0::isFinalDestructor>(tags);
}

inline void write(Bits<FnFlags0> const& bits, XMLTags& tags)
{
    WriteBits(bits).write<FnFlags0::isVariadic>(tags);
    WriteBits(bits).write<FnFlags0::isVirtualAsWritten>(tags);
    WriteBits(bits).write<FnFlags0::isPure>(tags);
    WriteBits(bits).write<FnFlags0::isDefaulted>(tags);
    WriteBits(bits).write<FnFlags0::isExplicitlyDefaulted>(tags);
    WriteBits(bits).write<FnFlags0::isDeleted>(tags);
    WriteBits(bits).write<FnFlags0::isDeletedAsWritten>(tags);
    WriteBits(bits).write<FnFlags0::isNoReturn>(tags);
    WriteBits(bits).write<FnFlags0::hasOverrideAttr>(tags);
    WriteBits(bits).write<FnFlags0::hasTrailingReturn>(tags);
    WriteBits(bits).write<FnFlags0::isConst>(tags);
    WriteBits(bits).write<FnFlags0::isVolatile>(tags);

    WriteBits(bits).write<FnFlags0::constexprKind, ConstexprSpecKind>(tags);
    WriteBits(bits).write<FnFlags0::exceptionSpecType, ExceptionSpecificationType>(tags);
    WriteBits(bits).write<FnFlags0::overloadedOperator, OverloadedOperatorKind>(tags);
    WriteBits(bits).write<FnFlags0::storageClass, StorageClass>(tags);
    WriteBits(bits).write<FnFlags0::refQualifier, RefQualifierKind>(tags);
}

inline void write(Bits<FnFlags1> const& bits, XMLTags& tags)
{
    WriteBits(bits).write<FnFlags1::isNodiscard>(tags);
    WriteBits(bits).write<FnFlags1::nodiscardSpelling, WarnUnusedResultAttr::Spelling>(tags);
    WriteBits(bits).write<FnFlags1::isExplicit>(tags);
}

inline void write(Bits<VarFlags0> const& bits, XMLTags& tags)
{
    WriteBits(bits).write<VarFlags0::storageClass, StorageClass>(tags);
}

inline void writeReturnType(TypeInfo const& I, XMLTags& tags)
{
    if(I.Type.Name == "void")
        return;
    tags.write(returnTagName, {}, {
        { "type", I.Type.Name },
        { I.Type.id }
        });
}

inline void writeParam(FieldTypeInfo const& I, XMLTags& tags)
{
    tags.write(paramTagName, {}, {
        { "name", I.Name, ! I.Name.empty() },
        { "type", I.Type.Name },
        { "default", I.DefaultValue, ! I.DefaultValue.empty() },
        { I.Type.id } });
}

/** Return the xml tag name for the Info.
*/
llvm::StringRef
getTagName(Info const& I) noexcept;

} // xml
} // mrdox
} // clang

#endif
