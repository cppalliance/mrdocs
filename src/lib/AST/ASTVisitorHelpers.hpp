// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_AST_ASTVISITORHELPERS_HPP
#define MRDOCS_LIB_AST_ASTVISITORHELPERS_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Support/TypeTraits.hpp>
#include <clang/AST/AST.h>
#include <clang/AST/Attr.h>
#include <clang/AST/DeclFriend.h>
#include <clang/AST/DeclOpenMP.h>
#include <type_traits>

namespace clang {
namespace mrdocs {
namespace {

/** Determine the MrDocs Info type for a Clang DeclType
 */
template <class DeclType>
struct MrDocsType : std::type_identity<Info> {};

template <std::derived_from<CXXRecordDecl> DeclType>
struct MrDocsType<DeclType> : std::type_identity<RecordInfo> {};

template <std::derived_from<VarDecl> VarTy>
struct MrDocsType<VarTy> : std::type_identity<VariableInfo> {};

template <std::derived_from<FunctionDecl> FunctionTy>
struct MrDocsType<FunctionTy> : std::type_identity<FunctionInfo> {};

template <std::derived_from<TypedefNameDecl> TypedefNameTy>
struct MrDocsType<TypedefNameTy> : std::type_identity<TypedefInfo> {};

template <>
struct MrDocsType<EnumDecl> : std::type_identity<EnumInfo> {};

template <>
struct MrDocsType<FieldDecl> : std::type_identity<FieldInfo> {};

template <>
struct MrDocsType<EnumConstantDecl> : std::type_identity<EnumeratorInfo> {};

template <>
struct MrDocsType<FriendDecl> : std::type_identity<FriendInfo> {};

template <>
struct MrDocsType<CXXDeductionGuideDecl> : std::type_identity<GuideInfo> {};

template <>
struct MrDocsType<NamespaceAliasDecl> : std::type_identity<AliasInfo> {};

template <>
struct MrDocsType<UsingDirectiveDecl> : std::type_identity<UsingInfo> {};

template <>
struct MrDocsType<UsingDecl> : std::type_identity<UsingInfo> {};

/// @copydoc MrDocsType
template <class DeclType>
using MrDocsType_t = typename MrDocsType<DeclType>::type;

/** Convert a Clang AccessSpecifier into a MrDocs AccessKind
 */
AccessKind
convertToAccessKind(
    AccessSpecifier spec)
{
    using OldKind = AccessSpecifier;
    using NewKind = AccessKind;
    switch(spec)
    {
    case OldKind::AS_public:    return NewKind::Public;
    case OldKind::AS_protected: return NewKind::Protected;
    case OldKind::AS_private:   return NewKind::Private;
    case OldKind::AS_none:      return NewKind::None;
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Convert a Clang StorageClass into a MrDocs StorageClassKind
 */
StorageClassKind
convertToStorageClassKind(
    StorageClass spec)
{
    using OldKind = StorageClass;
    using NewKind = StorageClassKind;
    switch(spec)
    {
    case OldKind::SC_None:     return NewKind::None;
    case OldKind::SC_Extern:   return NewKind::Extern;
    case OldKind::SC_Static:   return NewKind::Static;
    case OldKind::SC_Auto:     return NewKind::Auto;
    case OldKind::SC_Register: return NewKind::Register;
    default:
        // SC_PrivateExtern (__private_extern__)
        // is a C only Apple extension
        MRDOCS_UNREACHABLE();
    }
}

/** Convert a Clang ConstexprSpecKind into a MrDocs ConstexprKind
 */
ConstexprKind
convertToConstexprKind(
    ConstexprSpecKind spec)
{
    using OldKind = ConstexprSpecKind;
    using NewKind = ConstexprKind;
    switch(spec)
    {
    case OldKind::Unspecified: return NewKind::None;
    case OldKind::Constexpr:   return NewKind::Constexpr;
    case OldKind::Consteval:   return NewKind::Consteval;
    // KRYSTIAN NOTE: ConstexprSpecKind::Constinit exists,
    // but I don't think it's ever used because a variable
    // can be declared both constexpr and constinit
    // (but not both in the same declaration)
    case OldKind::Constinit:
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Convert a Clang ExplicitSpecKind into a MrDocs ExplicitKind
 */
ExplicitKind
convertToExplicitKind(
    const ExplicitSpecifier& spec)
{
    using OldKind = ExplicitSpecKind;
    using NewKind = ExplicitKind;

    // no explicit-specifier
    if(! spec.isSpecified())
        return NewKind::None;

    switch(spec.getKind())
    {
    case OldKind::ResolvedFalse:
        return NewKind::ExplicitFalse;
    case OldKind::ResolvedTrue:
        if(spec.getExpr())
            return NewKind::ExplicitTrue;
        // explicit-specifier without constant-expression
        return NewKind::Explicit;
    case OldKind::Unresolved:
        return NewKind::ExplicitUnresolved;
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Convert a Clang ExceptionSpecificationType into a MrDocs NoexceptKind
 */
NoexceptKind
convertToNoexceptKind(
    ExceptionSpecificationType spec)
{
    using OldKind = ExceptionSpecificationType;
    using NewKind = NoexceptKind;
    // KRYSTIAN TODO: right now we convert pre-C++17 dynamic exception
    // specifications to an (roughly) equivalent noexcept-specifier
    switch(spec)
    {
    case OldKind::EST_None:
    case OldKind::EST_MSAny:
    case OldKind::EST_Unevaluated:
    case OldKind::EST_Uninstantiated:
    // we *shouldn't* ever encounter an unparsed exception specification,
    // assuming that clang is working correctly...
    case OldKind::EST_Unparsed:
    case OldKind::EST_Dynamic:
    case OldKind::EST_NoexceptFalse:
        return NewKind::False;
    case OldKind::EST_NoThrow:
    case OldKind::EST_BasicNoexcept:
    case OldKind::EST_NoexceptTrue:
    case OldKind::EST_DynamicNone:
        return NewKind::True;
    case OldKind::EST_DependentNoexcept:
        return NewKind::Dependent;
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Convert a Clang OverloadedOperatorKind into a MrDocs OperatorKind
 */
OperatorKind
convertToOperatorKind(
    OverloadedOperatorKind kind)
{
    using OldKind = OverloadedOperatorKind;
    using NewKind = OperatorKind;
    switch(kind)
    {
    case OldKind::OO_None:                return NewKind::None;
    case OldKind::OO_New:                 return NewKind::New;
    case OldKind::OO_Delete:              return NewKind::Delete;
    case OldKind::OO_Array_New:           return NewKind::ArrayNew;
    case OldKind::OO_Array_Delete:        return NewKind::ArrayDelete;
    case OldKind::OO_Plus:                return NewKind::Plus;
    case OldKind::OO_Minus:               return NewKind::Minus;
    case OldKind::OO_Star:                return NewKind::Star;
    case OldKind::OO_Slash:               return NewKind::Slash;
    case OldKind::OO_Percent:             return NewKind::Percent;
    case OldKind::OO_Caret:               return NewKind::Caret;
    case OldKind::OO_Amp:                 return NewKind::Amp;
    case OldKind::OO_Pipe:                return NewKind::Pipe;
    case OldKind::OO_Tilde:               return NewKind::Tilde;
    case OldKind::OO_Exclaim:             return NewKind::Exclaim;
    case OldKind::OO_Equal:               return NewKind::Equal;
    case OldKind::OO_Less:                return NewKind::Less;
    case OldKind::OO_Greater:             return NewKind::Greater;
    case OldKind::OO_PlusEqual:           return NewKind::PlusEqual;
    case OldKind::OO_MinusEqual:          return NewKind::MinusEqual;
    case OldKind::OO_StarEqual:           return NewKind::StarEqual;
    case OldKind::OO_SlashEqual:          return NewKind::SlashEqual;
    case OldKind::OO_PercentEqual:        return NewKind::PercentEqual;
    case OldKind::OO_CaretEqual:          return NewKind::CaretEqual;
    case OldKind::OO_AmpEqual:            return NewKind::AmpEqual;
    case OldKind::OO_PipeEqual:           return NewKind::PipeEqual;
    case OldKind::OO_LessLess:            return NewKind::LessLess;
    case OldKind::OO_GreaterGreater:      return NewKind::GreaterGreater;
    case OldKind::OO_LessLessEqual:       return NewKind::LessLessEqual;
    case OldKind::OO_GreaterGreaterEqual: return NewKind::GreaterGreaterEqual;
    case OldKind::OO_EqualEqual:          return NewKind::EqualEqual;
    case OldKind::OO_ExclaimEqual:        return NewKind::ExclaimEqual;
    case OldKind::OO_LessEqual:           return NewKind::LessEqual;
    case OldKind::OO_GreaterEqual:        return NewKind::GreaterEqual;
    case OldKind::OO_Spaceship:           return NewKind::Spaceship;
    case OldKind::OO_AmpAmp:              return NewKind::AmpAmp;
    case OldKind::OO_PipePipe:            return NewKind::PipePipe;
    case OldKind::OO_PlusPlus:            return NewKind::PlusPlus;
    case OldKind::OO_MinusMinus:          return NewKind::MinusMinus;
    case OldKind::OO_Comma:               return NewKind::Comma;
    case OldKind::OO_ArrowStar:           return NewKind::ArrowStar;
    case OldKind::OO_Arrow:               return NewKind::Arrow;
    case OldKind::OO_Call:                return NewKind::Call;
    case OldKind::OO_Subscript:           return NewKind::Subscript;
    case OldKind::OO_Conditional:         return NewKind::Conditional;
    case OldKind::OO_Coawait:             return NewKind::Coawait;
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Convert a Clang ReferenceKind into a MrDocs ReferenceKind
 */
ReferenceKind
convertToReferenceKind(
    RefQualifierKind kind)
{
    using OldKind = RefQualifierKind;
    using NewKind = ReferenceKind;
    switch(kind)
    {
    case OldKind::RQ_None:   return NewKind::None;
    case OldKind::RQ_LValue: return NewKind::LValue;
    case OldKind::RQ_RValue: return NewKind::RValue;
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Convert a Clang TagTypeKind into a MrDocs RecordKeyKind
 */
RecordKeyKind
convertToRecordKeyKind(
    TagTypeKind kind)
{
    using OldKind = TagTypeKind;
    using NewKind = RecordKeyKind;
    switch(kind)
    {
    case OldKind::TTK_Struct: return NewKind::Struct;
    case OldKind::TTK_Class:  return NewKind::Class;
    case OldKind::TTK_Union:  return NewKind::Union;
    default:
        // unsupported TagTypeKind
        MRDOCS_UNREACHABLE();
    }
}

/** Convert a Clang unsigned qualifier kind into a MrDocs QualifierKind
 */
QualifierKind
convertToQualifierKind(
    unsigned quals)
{
    std::underlying_type_t<
        QualifierKind> result = QualifierKind::None;
    if(quals & Qualifiers::Const)
        result |= QualifierKind::Const;
    if(quals & Qualifiers::Volatile)
        result |= QualifierKind::Volatile;
    return static_cast<QualifierKind>(result);

}

/** Convert a Clang Decl::Kind into a MrDocs FunctionClass
 */
FunctionClass
convertToFunctionClass(
    Decl::Kind kind)
{
    using OldKind = Decl::Kind;
    using NewKind = FunctionClass;
    switch(kind)
    {
    case OldKind::Function:          return NewKind::Normal;
    case OldKind::CXXMethod:         return NewKind::Normal;
    case OldKind::CXXConstructor:    return NewKind::Constructor;
    case OldKind::CXXConversion:     return NewKind::Conversion;
    case OldKind::CXXDestructor:     return NewKind::Destructor;
    default:
        MRDOCS_UNREACHABLE();
    }
}

// ----------------------------------------------------------------

/** Visit a Decl and call the appropriate visitor function.
 */
template<
    typename DeclTy,
    typename Visitor,
    typename... Args>
    requires std::derived_from<DeclTy, Decl>
decltype(auto)
visit(
    DeclTy* D,
    Visitor&& visitor,
    Args&&... args)
{
    MRDOCS_ASSERT(D);
    switch(D->getKind())
    {
    #define ABSTRACT_DECL(TYPE)
    #define DECL(DERIVED, BASE) \
        case Decl::DERIVED: \
        if constexpr(std::derived_from<DERIVED##Decl, DeclTy>) \
            return std::forward<Visitor>(visitor)( \
                static_cast<add_cv_from_t<DeclTy, \
                    DERIVED##Decl>*>(D), \
                std::forward<Args>(args)...); \
        else \
            MRDOCS_UNREACHABLE();

    #include <clang/AST/DeclNodes.inc>

    default:
        MRDOCS_UNREACHABLE();
    }
}

template<typename DeclTy>
consteval
Decl::Kind
DeclToKindImpl() = delete;

#define ABSTRACT_DECL(TYPE)
#define DECL(DERIVED, BASE) \
    template<> \
    consteval \
    Decl::Kind \
    DeclToKindImpl<DERIVED##Decl>() { return Decl::DERIVED; }

#include <clang/AST/DeclNodes.inc>

/** Get the Decl::Kind for a type DeclTy derived from Decl.
 */
template<typename DeclTy>
consteval
Decl::Kind
DeclToKind()
{
    return DeclToKindImpl<
        std::remove_cvref_t<DeclTy>>();
}

// ----------------------------------------------------------------

/** Visit a Type and call the appropriate visitor function.
 */
template<
    typename TypeTy,
    typename Visitor,
    typename... Args>
    requires std::derived_from<TypeTy, Type>
decltype(auto)
visit(
    TypeTy* T,
    Visitor&& visitor,
    Args&&... args)
{
    #define ABSTRACT_TYPE(DERIVED, BASE)
    switch(T->getTypeClass())
    {
    #define TYPE(DERIVED, BASE) \
        case Type::DERIVED: \
        if constexpr(std::derived_from<DERIVED##Type, TypeTy>) \
            return std::forward<Visitor>(visitor)( \
                static_cast<add_cv_from_t<TypeTy, \
                    DERIVED##Type>*>(T), \
                std::forward<Args>(args)...); \
        else \
            MRDOCS_UNREACHABLE();

    #include <clang/AST/TypeNodes.inc>

    default:
        MRDOCS_UNREACHABLE();
    }
}

template<typename TypeTy>
consteval
Type::TypeClass
TypeToKindImpl() = delete;

#define ABSTRACT_TYPE(DERIVED, BASE)
#define TYPE(DERIVED, BASE) \
    template<> \
    consteval \
    Type::TypeClass \
    TypeToKindImpl<DERIVED##Type>() { return Type::DERIVED; }

#include <clang/AST/TypeNodes.inc>

template<typename TypeTy>
consteval
Type::TypeClass
TypeToKind()
{
    return TypeToKindImpl<
        std::remove_cvref_t<TypeTy>>();
}

// ----------------------------------------------------------------

/** Visit a TypeLoc and call the appropriate visitor function.
 */
template<
    typename TypeLocTy,
    typename Visitor,
    typename... Args>
    requires std::derived_from<TypeLocTy, TypeLoc>
decltype(auto)
visit(
    TypeLocTy* T,
    Visitor&& visitor,
    Args&&... args)
{
    switch(T->getTypeLocClass())
    {
    #define ABSTRACT_TYPELOC(DERIVED, BASE)
    #define TYPELOC(DERIVED, BASE) \
        case TypeLoc::DERIVED: \
        if constexpr(std::derived_from<DERIVED##TypeLoc, TypeLocTy>) \
            return std::forward<Visitor>(visitor)( \
                static_cast<add_cv_from_t<TypeLocTy, \
                    DERIVED##TypeLoc>*>(T), \
                std::forward<Args>(args)...); \
        else \
            MRDOCS_UNREACHABLE();

    #include <clang/AST/TypeLocNodes.def>

    default:
        MRDOCS_UNREACHABLE();
    }
}

template<typename TypeLocTy>
consteval
TypeLoc::TypeLocClass
TypeLocToKindImpl() = delete;

#define ABSTRACT_TYPELOC(DERIVED, BASE)
#define TYPELOC(DERIVED, BASE) \
    template<> \
    consteval \
    TypeLoc::TypeLocClass \
    TypeLocToKindImpl<DERIVED##TypeLoc>() { return TypeLoc::DERIVED; }

#include <clang/AST/TypeLocNodes.def>

/** Get the TypeLoc::TypeLocClass for a type TypeLocTy derived from TypeLoc.
 */
template<typename TypeLocTy>
consteval
TypeLoc::TypeLocClass
TypeLocToKind()
{
    return TypeLocToKindImpl<
        std::remove_cvref_t<TypeLocTy>>();
}

} // (anon)
} // mrdocs
} // clang

#endif
