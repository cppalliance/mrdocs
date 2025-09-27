//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_AST_CLANGHELPERS_HPP
#define MRDOCS_LIB_AST_CLANGHELPERS_HPP

#include <mrdocs/Platform.hpp>
#include <lib/AST/InstantiatedFromVisitor.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Metadata/Symbol/SymbolID.hpp>
#include <mrdocs/Metadata/Type/QualifierKind.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/TypeTraits.hpp>
#include <clang/AST/AST.h>
#include <clang/AST/DeclFriend.h>
#include <clang/AST/DeclOpenMP.h>
#include <clang/AST/Expr.h>
#include <clang/Sema/Sema.h>
#include <type_traits>

namespace mrdocs {

/** Substitute the constraint expression without satisfaction.

    This function substitutes the constraint expression without
    checking for satisfaction.

    It uses the provided Sema object and template comparison information
    to perform the substitution.

    @param S The Sema object used for semantic analysis.
    @param DeclInfo The template comparison information for the new declaration.
    @param ConstrExpr The constraint expression to be substituted.
    @return The substituted constraint expression, or nullptr if an error occurs.
 */
clang::Expr const*
SubstituteConstraintExpressionWithoutSatisfaction(
    clang::Sema &S,
    const clang::Sema::TemplateCompareNewDeclInfo &DeclInfo,
    const clang::Expr *ConstrExpr);

/** Determine the MrDocs Info type for a Clang DeclType

    This trait associates a Clang Decl type with the corresponding
    MrDocs Info type, when there is a direct correspondence.

    This is used to determine what type of Info object to create
    and return when @ref ASTVisitor::upsertMrDocsInfoFor is called
    to create or update an Info object for a Decl.

    Not all Info types have a direct correspondence with a Decl type.
    In this case, the objects are created and updated by the visitor
    at other steps in the traversal.

    When there's no direct correspondence, this trait returns
    the base Info type.
 */
template <class>
struct InfoTypeFor {};

// Extract NamespaceSymbol from NamespaceDecl or TranslationUnitDecl
template <>
struct InfoTypeFor<clang::NamespaceDecl>
    : std::type_identity<NamespaceSymbol> {};

template <>
struct InfoTypeFor<clang::TranslationUnitDecl>
    : std::type_identity<NamespaceSymbol> {};

// Extract RecordSymbol from anything derived from clang::CXXRecordDecl
// and clang::ClassTemplateDecl. Decls derived from clang::CXXRecordDecl
// include class specializations.
template <>
struct InfoTypeFor<clang::CXXRecordDecl>
    : std::type_identity<RecordSymbol> {};

template <>
struct InfoTypeFor<clang::ClassTemplateSpecializationDecl>
    : std::type_identity<RecordSymbol> {};

template <>
struct InfoTypeFor<clang::ClassTemplatePartialSpecializationDecl>
    : std::type_identity<RecordSymbol> {};

template <>
struct InfoTypeFor<clang::ClassTemplateDecl>
    : std::type_identity<RecordSymbol> {};

// Extract FunctionSymbol from anything derived from clang::FunctionDecl
template <>
struct InfoTypeFor<clang::FunctionDecl>
    : std::type_identity<FunctionSymbol> {};

template <>
struct InfoTypeFor<clang::CXXMethodDecl>
    : std::type_identity<FunctionSymbol> {};

template <>
struct InfoTypeFor<clang::CXXConstructorDecl>
    : std::type_identity<FunctionSymbol> {};

template <>
struct InfoTypeFor<clang::CXXDestructorDecl>
    : std::type_identity<FunctionSymbol> {};

template <>
struct InfoTypeFor<clang::CXXConversionDecl>
    : std::type_identity<FunctionSymbol> {};

template <>
struct InfoTypeFor<clang::FunctionTemplateDecl>
    : std::type_identity<FunctionSymbol> {};

// Extract EnumSymbol from EnumDecl
template <>
struct InfoTypeFor<clang::EnumDecl>
    : std::type_identity<EnumSymbol> {};

// Extract EnumConstantSymbol from EnumConstantDecl
template <>
struct InfoTypeFor<clang::EnumConstantDecl>
    : std::type_identity<EnumConstantSymbol> {};

// Extract TypedefSymbol from anything derived from TypedefNameDecl
template <>
struct InfoTypeFor<clang::TypedefDecl>
    : std::type_identity<TypedefSymbol> {};

template <>
struct InfoTypeFor<clang::TypeAliasDecl>
    : std::type_identity<TypedefSymbol> {};

template <>
struct InfoTypeFor<clang::TypedefNameDecl>
    : std::type_identity<TypedefSymbol> {};

template <>
struct InfoTypeFor<clang::TypeAliasTemplateDecl>
    : std::type_identity<TypedefSymbol> {};

// Extract VariableSymbol from anything derived from VarDecl
// and VarTemplateDecl.
template <>
struct InfoTypeFor<clang::VarDecl>
    : std::type_identity<VariableSymbol> {};

template <>
struct InfoTypeFor<clang::VarTemplateSpecializationDecl>
    : std::type_identity<VariableSymbol> {};

template <>
struct InfoTypeFor<clang::VarTemplatePartialSpecializationDecl>
    : std::type_identity<VariableSymbol> {};

template <>
struct InfoTypeFor<clang::VarTemplateDecl>
    : std::type_identity<VariableSymbol> {};

template <>
struct InfoTypeFor<clang::FieldDecl>
    : std::type_identity<VariableSymbol> {};

// Extract GuideSymbol from CXXDeductionGuideDecl
template <>
struct InfoTypeFor<clang::CXXDeductionGuideDecl>
    : std::type_identity<GuideSymbol> {};

// Extract NamespaceAliasSymbol from NamespaceAliasDecl
template <>
struct InfoTypeFor<clang::NamespaceAliasDecl>
    : std::type_identity<NamespaceAliasSymbol> {};

// Extract UsingSymbol from UsingDecl
template <>
struct InfoTypeFor<clang::UsingDecl>
    : std::type_identity<UsingSymbol> {};

// Extract ConceptSymbol from ConceptDecl
template <>
struct InfoTypeFor<clang::ConceptDecl>
    : std::type_identity<ConceptSymbol> {};

/// Determine if there's a MrDocs Info type for a Clang DeclType
template <class T>
concept HasInfoTypeFor = std::derived_from<T, clang::Decl> && requires
{
    typename InfoTypeFor<T>::type;
};

/// @copydoc InfoTypeFor
template <class DeclType>
using InfoTypeFor_t = typename InfoTypeFor<DeclType>::type;

/** Convert a Clang AccessSpecifier into a MrDocs AccessKind
 */
inline
AccessKind
toAccessKind(clang::AccessSpecifier const spec)
{
    switch(spec)
    {
    case clang::AccessSpecifier::AS_public:    return AccessKind::Public;
    case clang::AccessSpecifier::AS_protected: return AccessKind::Protected;
    case clang::AccessSpecifier::AS_private:   return AccessKind::Private;
    case clang::AccessSpecifier::AS_none:      return AccessKind::None;
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Convert a Clang StorageClass into a MrDocs StorageClassKind
 */
inline
StorageClassKind
toStorageClassKind(clang::StorageClass const spec)
{
    switch(spec)
    {
    case clang::StorageClass::SC_None:     return StorageClassKind::None;
    case clang::StorageClass::SC_Extern:   return StorageClassKind::Extern;
    case clang::StorageClass::SC_Static:   return StorageClassKind::Static;
    case clang::StorageClass::SC_Auto:     return StorageClassKind::Auto;
    case clang::StorageClass::SC_Register: return StorageClassKind::Register;
    default:
        // SC_PrivateExtern (__private_extern__)
        // is a C only Apple extension
        MRDOCS_UNREACHABLE();
    }
}

/** Convert a Clang ConstexprSpecKind into a MrDocs ConstexprKind
 */
inline
ConstexprKind
toConstexprKind(clang::ConstexprSpecKind const spec)
{
    switch(spec)
    {
    case clang::ConstexprSpecKind::Unspecified: return ConstexprKind::None;
    case clang::ConstexprSpecKind::Constexpr:   return ConstexprKind::Constexpr;
    case clang::ConstexprSpecKind::Consteval:   return ConstexprKind::Consteval;
    // KRYSTIAN NOTE: clang::ConstexprSpecKind::Constinit exists,
    // but I don't think it's ever used because a variable
    // can be declared both constexpr and constinit
    // (but not both in the same declaration)
    case clang::ConstexprSpecKind::Constinit:
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Convert a Clang ExplicitSpecKind into a MrDocs ExplicitKind
 */
inline
ExplicitKind
toExplicitKind(clang::ExplicitSpecifier const& spec)
{
    // no explicit-specifier
    if (!spec.isSpecified())
    {
        return ExplicitKind::False;
    }

    switch(spec.getKind())
    {
    case clang::ExplicitSpecKind::ResolvedFalse:  return ExplicitKind::False;
    case clang::ExplicitSpecKind::ResolvedTrue:   return ExplicitKind::True;
    case clang::ExplicitSpecKind::Unresolved:     return ExplicitKind::Dependent;
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Convert a Clang ExceptionSpecificationType into a MrDocs NoexceptKind
 */
inline
NoexceptKind
toNoexceptKind(clang::ExceptionSpecificationType const spec)
{
    // KRYSTIAN TODO: right now we convert pre-C++17 dynamic exception
    // specifications to an (roughly) equivalent noexcept-specifier
    switch(spec)
    {
    case clang::ExceptionSpecificationType::EST_None:
    case clang::ExceptionSpecificationType::EST_MSAny:
    case clang::ExceptionSpecificationType::EST_Unevaluated:
    case clang::ExceptionSpecificationType::EST_Uninstantiated:
    // we *shouldn't* ever encounter an unparsed exception specification,
    // assuming that clang is working correctly...
    case clang::ExceptionSpecificationType::EST_Unparsed:
    case clang::ExceptionSpecificationType::EST_Dynamic:
    case clang::ExceptionSpecificationType::EST_NoexceptFalse:
        return NoexceptKind::False;
    case clang::ExceptionSpecificationType::EST_NoThrow:
    case clang::ExceptionSpecificationType::EST_BasicNoexcept:
    case clang::ExceptionSpecificationType::EST_NoexceptTrue:
    case clang::ExceptionSpecificationType::EST_DynamicNone:
        return NoexceptKind::True;
    case clang::ExceptionSpecificationType::EST_DependentNoexcept:
        return NoexceptKind::Dependent;
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Convert a Clang clang::OverloadedOperatorKind into a MrDocs OperatorKind
 */
inline
OperatorKind
toOperatorKind(clang::OverloadedOperatorKind const kind)
{
    switch(kind)
    {
    case clang::OverloadedOperatorKind::OO_None:
        return OperatorKind::None;
    case clang::OverloadedOperatorKind::OO_New:
        return OperatorKind::New;
    case clang::OverloadedOperatorKind::OO_Delete:
        return OperatorKind::Delete;
    case clang::OverloadedOperatorKind::OO_Array_New:
        return OperatorKind::ArrayNew;
    case clang::OverloadedOperatorKind::OO_Array_Delete:
        return OperatorKind::ArrayDelete;
    case clang::OverloadedOperatorKind::OO_Plus:
        return OperatorKind::Plus;
    case clang::OverloadedOperatorKind::OO_Minus:
        return OperatorKind::Minus;
    case clang::OverloadedOperatorKind::OO_Star:
        return OperatorKind::Star;
    case clang::OverloadedOperatorKind::OO_Slash:
        return OperatorKind::Slash;
    case clang::OverloadedOperatorKind::OO_Percent:
        return OperatorKind::Percent;
    case clang::OverloadedOperatorKind::OO_Caret:
        return OperatorKind::Caret;
    case clang::OverloadedOperatorKind::OO_Amp:
        return OperatorKind::Amp;
    case clang::OverloadedOperatorKind::OO_Pipe:
        return OperatorKind::Pipe;
    case clang::OverloadedOperatorKind::OO_Tilde:
        return OperatorKind::Tilde;
    case clang::OverloadedOperatorKind::OO_Exclaim:
        return OperatorKind::Exclaim;
    case clang::OverloadedOperatorKind::OO_Equal:
        return OperatorKind::Equal;
    case clang::OverloadedOperatorKind::OO_Less:
        return OperatorKind::Less;
    case clang::OverloadedOperatorKind::OO_Greater:
        return OperatorKind::Greater;
    case clang::OverloadedOperatorKind::OO_PlusEqual:
        return OperatorKind::PlusEqual;
    case clang::OverloadedOperatorKind::OO_MinusEqual:
        return OperatorKind::MinusEqual;
    case clang::OverloadedOperatorKind::OO_StarEqual:
        return OperatorKind::StarEqual;
    case clang::OverloadedOperatorKind::OO_SlashEqual:
        return OperatorKind::SlashEqual;
    case clang::OverloadedOperatorKind::OO_PercentEqual:
        return OperatorKind::PercentEqual;
    case clang::OverloadedOperatorKind::OO_CaretEqual:
        return OperatorKind::CaretEqual;
    case clang::OverloadedOperatorKind::OO_AmpEqual:
        return OperatorKind::AmpEqual;
    case clang::OverloadedOperatorKind::OO_PipeEqual:
        return OperatorKind::PipeEqual;
    case clang::OverloadedOperatorKind::OO_LessLess:
        return OperatorKind::LessLess;
    case clang::OverloadedOperatorKind::OO_GreaterGreater:
        return OperatorKind::GreaterGreater;
    case clang::OverloadedOperatorKind::OO_LessLessEqual:
        return OperatorKind::LessLessEqual;
    case clang::OverloadedOperatorKind::OO_GreaterGreaterEqual:
        return OperatorKind::GreaterGreaterEqual;
    case clang::OverloadedOperatorKind::OO_EqualEqual:
        return OperatorKind::EqualEqual;
    case clang::OverloadedOperatorKind::OO_ExclaimEqual:
        return OperatorKind::ExclaimEqual;
    case clang::OverloadedOperatorKind::OO_LessEqual:
        return OperatorKind::LessEqual;
    case clang::OverloadedOperatorKind::OO_GreaterEqual:
        return OperatorKind::GreaterEqual;
    case clang::OverloadedOperatorKind::OO_Spaceship:
        return OperatorKind::Spaceship;
    case clang::OverloadedOperatorKind::OO_AmpAmp:
        return OperatorKind::AmpAmp;
    case clang::OverloadedOperatorKind::OO_PipePipe:
        return OperatorKind::PipePipe;
    case clang::OverloadedOperatorKind::OO_PlusPlus:
        return OperatorKind::PlusPlus;
    case clang::OverloadedOperatorKind::OO_MinusMinus:
        return OperatorKind::MinusMinus;
    case clang::OverloadedOperatorKind::OO_Comma:
        return OperatorKind::Comma;
    case clang::OverloadedOperatorKind::OO_ArrowStar:
        return OperatorKind::ArrowStar;
    case clang::OverloadedOperatorKind::OO_Arrow:
        return OperatorKind::Arrow;
    case clang::OverloadedOperatorKind::OO_Call:
        return OperatorKind::Call;
    case clang::OverloadedOperatorKind::OO_Subscript:
        return OperatorKind::Subscript;
    case clang::OverloadedOperatorKind::OO_Conditional:
        return OperatorKind::Conditional;
    case clang::OverloadedOperatorKind::OO_Coawait:
        return OperatorKind::Coawait;
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Convert a Clang ReferenceKind into a MrDocs ReferenceKind
 */
inline
ReferenceKind
toReferenceKind(clang::RefQualifierKind const kind)
{
    switch(kind)
    {
    case clang::RefQualifierKind::RQ_None:
        return ReferenceKind::None;
    case clang::RefQualifierKind::RQ_LValue:
        return ReferenceKind::LValue;
    case clang::RefQualifierKind::RQ_RValue:
        return ReferenceKind::RValue;
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Convert a Clang clang::TagTypeKind into a MrDocs RecordKeyKind
 */
inline
RecordKeyKind
toRecordKeyKind(clang::TagTypeKind const kind)
{
    switch(kind)
    {
    case clang::TagTypeKind::Struct: return RecordKeyKind::Struct;
    case clang::TagTypeKind::Class:  return RecordKeyKind::Class;
    case clang::TagTypeKind::Union:  return RecordKeyKind::Union;
    default:
        // unsupported clang::TagTypeKind (Interface, or Enum)
        MRDOCS_UNREACHABLE();
    }
}

/** Convert a Clang unsigned qualifier kind into a MrDocs QualifierKind
 */
inline
QualifierKind
toQualifierKind(unsigned const quals)
{
    std::underlying_type_t<QualifierKind> result = QualifierKind::None;
    if (quals & clang::Qualifiers::Const)
    {
        result |= QualifierKind::Const;
    }
    if (quals & clang::Qualifiers::Volatile)
    {
        result |= QualifierKind::Volatile;
    }
    return static_cast<QualifierKind>(result);

}

/** Convert a Clang Decl::Kind into a MrDocs FunctionClass
 */
inline
FunctionClass
toFunctionClass(clang::Decl::Kind const kind)
{
    switch(kind)
    {
    case clang::Decl::Kind::Function:
    case clang::Decl::Kind::CXXMethod:         return FunctionClass::Normal;
    case clang::Decl::Kind::CXXConstructor:    return FunctionClass::Constructor;
    case clang::Decl::Kind::CXXConversion:     return FunctionClass::Conversion;
    case clang::Decl::Kind::CXXDestructor:     return FunctionClass::Destructor;
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Convert a Clang clang::AutoTypeKeyword into a MrDocs AutoKind
 */
inline
AutoKind
toAutoKind(clang::AutoTypeKeyword const kind)
{
    switch(kind)
    {
    case clang::AutoTypeKeyword::Auto:
    case clang::AutoTypeKeyword::GNUAutoType:
      return AutoKind::Auto;
    case clang::AutoTypeKeyword::DecltypeAuto:
      return AutoKind::DecltypeAuto;
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Convert a Clang clang::clang::BuiltinType into a MrDocs AutoKind
 */
inline
Optional<FundamentalTypeKind>
toFundamentalTypeKind(clang::BuiltinType::Kind const kind)
{
    switch(kind)
    {
    case clang::BuiltinType::Kind::Void:
        return FundamentalTypeKind::Void;
    case clang::BuiltinType::Kind::NullPtr:
        return FundamentalTypeKind::Nullptr;
    case clang::BuiltinType::Kind::Bool:
        return FundamentalTypeKind::Bool;
    case clang::BuiltinType::Kind::Char_U:
    case clang::BuiltinType::Kind::Char_S:
        return FundamentalTypeKind::Char;
    case clang::BuiltinType::Kind::SChar:
        return FundamentalTypeKind::SignedChar;
    case clang::BuiltinType::Kind::UChar:
        return FundamentalTypeKind::UnsignedChar;
    case clang::BuiltinType::Kind::Char8:
        return FundamentalTypeKind::Char8;
    case clang::BuiltinType::Kind::Char16:
        return FundamentalTypeKind::Char16;
    case clang::BuiltinType::Kind::Char32:
        return FundamentalTypeKind::Char32;
    case clang::BuiltinType::Kind::WChar_S:
    case clang::BuiltinType::Kind::WChar_U:
        return FundamentalTypeKind::WChar;
    case clang::BuiltinType::Kind::Short:
        return FundamentalTypeKind::Short;
    case clang::BuiltinType::Kind::UShort:
        return FundamentalTypeKind::UnsignedShort;
    case clang::BuiltinType::Kind::Int:
        return FundamentalTypeKind::Int;
    case clang::BuiltinType::Kind::UInt:
        return FundamentalTypeKind::UnsignedInt;
    case clang::BuiltinType::Kind::Long:
        return FundamentalTypeKind::Long;
    case clang::BuiltinType::Kind::ULong:
        return FundamentalTypeKind::UnsignedLong;
    case clang::BuiltinType::Kind::LongLong:
        return FundamentalTypeKind::LongLong;
    case clang::BuiltinType::Kind::ULongLong:
        return FundamentalTypeKind::UnsignedLongLong;
    case clang::BuiltinType::Kind::Float:
        return FundamentalTypeKind::Float;
    case clang::BuiltinType::Kind::Double:
        return FundamentalTypeKind::Double;
    case clang::BuiltinType::Kind::LongDouble:
        return FundamentalTypeKind::LongDouble;
    default:
        return std::nullopt;
    }
}

// ----------------------------------------------------------------

/** Visit a Decl and call the appropriate visitor function.
 */
template<
    typename DeclTy,
    typename Visitor,
    typename... Args>
    requires std::derived_from<DeclTy, clang::Decl>
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
        case clang::Decl::DERIVED: \
        if constexpr(std::derived_from<clang::DERIVED##Decl, DeclTy>) \
            return std::forward<Visitor>(visitor)( \
                static_cast<add_cv_from_t<DeclTy, \
                    clang::DERIVED##Decl>*>(D), \
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
clang::Decl::Kind
DeclToKindImpl() = delete;

#define ABSTRACT_DECL(TYPE)
#define DECL(DERIVED, BASE) \
    template<> \
    consteval \
    clang::Decl::Kind \
    DeclToKindImpl<clang::DERIVED##Decl>() { return clang::Decl::DERIVED; }

#include <clang/AST/DeclNodes.inc>

/** Get the clang::Decl::Kind for a type DeclTy derived from Decl.
 */
template<typename DeclTy>
consteval
clang::Decl::Kind
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
    requires std::derived_from<TypeTy, clang::Type>
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
        case clang::Type::DERIVED: \
        if constexpr(std::derived_from<clang::DERIVED##Type, TypeTy>) \
            return std::forward<Visitor>(visitor)( \
                static_cast<add_cv_from_t<TypeTy, \
                    clang::DERIVED##Type>*>(T), \
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
clang::Type::TypeClass
TypeToKindImpl() = delete;

#define ABSTRACT_TYPE(DERIVED, BASE)
#define TYPE(DERIVED, BASE) \
    template<> \
    consteval \
    clang::Type::TypeClass \
    TypeToKindImpl<clang::DERIVED##Type>() { return clang::Type::DERIVED; }

#include <clang/AST/TypeNodes.inc>

template<typename TypeTy>
consteval
clang::Type::TypeClass
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
    requires std::derived_from<TypeLocTy, clang::TypeLoc>
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
        case clang::TypeLoc::DERIVED: \
        if constexpr(std::derived_from<clang::DERIVED##TypeLoc, TypeLocTy>) \
            return std::forward<Visitor>(visitor)( \
                static_cast<add_cv_from_t<TypeLocTy, \
                    clang::DERIVED##TypeLoc>*>(T), \
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
clang::TypeLoc::TypeLocClass
TypeLocToKindImpl() = delete;

#define ABSTRACT_TYPELOC(DERIVED, BASE)
#define TYPELOC(DERIVED, BASE) \
    template<> \
    consteval \
    clang::TypeLoc::TypeLocClass \
    TypeLocToKindImpl<clang::DERIVED##TypeLoc>() { return clang::TypeLoc::DERIVED; }

#include <clang/AST/TypeLocNodes.def>

/** Get the clang::TypeLoc::TypeLocClass for a type TypeLocTy derived from TypeLoc.
 */
template<typename TypeLocTy>
consteval
clang::TypeLoc::TypeLocClass
TypeLocToKind()
{
    return TypeLocToKindImpl<
        std::remove_cvref_t<TypeLocTy>>();
}

/** Get the user-written `Decl` for a `Decl`

    Given a `Decl` `D`, `getInstantiatedFrom` will return the
    user-written `Decl` corresponding to `D`.

    For specializations which were implicitly instantiated,
    this will be whichever `Decl` was used as the pattern
    for instantiation.

    For instance, if `D` represents `std::vector<int>`, the
    user-written `Decl` will be the `std::vector` template.
*/
template <class DeclTy>
DeclTy const*
getInstantiatedFrom(DeclTy const* D)
{
    if (!D)
    {
        return nullptr;
    }
    clang::Decl const* resultDecl = InstantiatedFromVisitor().Visit(D);
    return cast<DeclTy>(resultDecl);
}

template <class DeclTy>
requires
    std::derived_from<DeclTy, clang::FunctionDecl> ||
    std::same_as<clang::FunctionTemplateDecl, std::remove_cv_t<DeclTy>>
clang::FunctionDecl const*
getInstantiatedFrom(DeclTy const* D)
{
    return dyn_cast_if_present<clang::FunctionDecl>(
        getInstantiatedFrom<clang::Decl>(D));
}

template <class DeclTy>
requires
    std::derived_from<DeclTy, clang::CXXRecordDecl> ||
    std::same_as<clang::ClassTemplateDecl, std::remove_cv_t<DeclTy>>
clang::CXXRecordDecl const*
getInstantiatedFrom(DeclTy const* D)
{
    return dyn_cast_if_present<clang::CXXRecordDecl>(
        getInstantiatedFrom<clang::Decl>(D));
}

template <class DeclTy>
requires
    std::derived_from<DeclTy, clang::VarDecl> ||
    std::same_as<clang::VarTemplateDecl, std::remove_cv_t<DeclTy>>
clang::VarDecl const*
getInstantiatedFrom(DeclTy const* D)
{
    return dyn_cast_if_present<clang::VarDecl>(
        getInstantiatedFrom<clang::Decl>(D));
}

template <class DeclTy>
requires
    std::derived_from<DeclTy, clang::TypedefNameDecl> ||
    std::same_as<clang::TypeAliasTemplateDecl, std::remove_cv_t<DeclTy>>
clang::TypedefNameDecl const*
getInstantiatedFrom(DeclTy const* D)
{
    return dyn_cast_if_present<clang::TypedefNameDecl>(
        getInstantiatedFrom<clang::Decl>(D));
}

/** Get the access specifier for a `Decl`

    Given a `Decl`, this function will analyze the parent
    context and return the access specifier for the declaration.
 */
MRDOCS_DECL
clang::AccessSpecifier
getAccess(clang::Decl const* D);

MRDOCS_DECL
clang::QualType
getDeclaratorType(clang::DeclaratorDecl const* DD);

/** Get the NonTypeTemplateParm of an expression

    This function will return the clang::NonTypeTemplateParmDecl
    corresponding to the expression `E` if it is a
    clang::NonTypeTemplateParmDecl. If the expression is not
    a clang::NonTypeTemplateParmDecl, the function will return
    nullptr.

    For instance, given the expression `x` in the following
    code snippet:

    @code
    template<int x>
    void f() {}
    @endcode

    the function will return the clang::NonTypeTemplateParmDecl
    corresponding to `x`, which is the template parameter
    of the function `f`.
 */
MRDOCS_DECL
clang::NonTypeTemplateParmDecl const*
getNTTPFromExpr(clang::Expr const* E, unsigned Depth);

// Get the parent declaration of a declaration
MRDOCS_DECL
clang::Decl const*
getParent(clang::Decl const* D);

MRDOCS_DECL
void
getQualifiedName(
    clang::NamedDecl const* ND,
    clang::raw_ostream &Out,
    clang::PrintingPolicy const& Policy);

// If D refers to an implicit instantiation of a template specialization,
// decay it to the clang::Decl of the primary template. The template arguments
// will be extracted separately as part of the Type.
// For instance, a clang::Decl to `S<0>` becomes a clang::Decl to `S`, unless `S<0>` is
// an explicit specialization of the primary template.
// This function also applies recursively to the parent of D so that
// the primary template is resolved for nested classes.
// For instance, a clang::Decl to `A<0>::S` becomes a clang::Decl to `A::S`, unless
// `A<0>` is an explicit specialization of the primary template.
MRDOCS_DECL
clang::Decl const*
decayToPrimaryTemplate(clang::Decl const* D);

// Iterate the Decl and check if this is a template specialization
// also considering the parent declarations. For instance,
// S<0>::M<0> is a template specialization of S<0> and M<0>.
// This function returns true if both S<0> and M<0> are implicit
// template specializations.
MRDOCS_DECL
bool
isAllImplicitSpecialization(clang::Decl const* D);

// Check if any component of D is an implicit specialization
MRDOCS_DECL
bool
isAnyImplicitSpecialization(clang::Decl const* D);

// Check if at least one component of D is explicit
inline
bool
isAnyExplicitSpecialization(clang::Decl const* D)
{
    return !isAllImplicitSpecialization(D);
}

// Check if all components are explicit
inline
bool
isAllExplicitSpecialization(clang::Decl const* D)
{
    return !isAnyImplicitSpecialization(D);
}

MRDOCS_DECL
bool
isVirtualMember(clang::Decl const* D);

MRDOCS_DECL
bool
isAnonymousNamespace(clang::Decl const* D);

MRDOCS_DECL
bool
isStaticFileLevelMember(clang::Decl const *D);

MRDOCS_DECL
bool
isDocumented(clang::Decl const *D);

MRDOCS_DECL
clang::RawComment const*
getDocumentation(clang::Decl const *D);

template <class DeclTy>
bool
isDefinition(DeclTy* D)
{
    if constexpr (requires {D->isThisDeclarationADefinition();})
    {
        return D->isThisDeclarationADefinition();
    }
    else
    {
        return false;
    }
}

#ifdef NDEBUG
#define MRDOCS_SYMBOL_TRACE(D, C)
#else
namespace detail {
    // concept to check if ID->printQualifiedName(
    // std::declval<llvm::raw_svector_ostream&>(),
    // getASTVisitor().context_.getPrintingPolicy());
    // is a valid expression
    template <class T>
    concept HasPrintQualifiedName = requires(T const& D, llvm::raw_svector_ostream& OS, clang::PrintingPolicy PP)
    {
        D.printQualifiedName(OS, PP);
    };

    template <class T>
    concept HasPrint = requires(T const& D, llvm::raw_svector_ostream& OS, clang::PrintingPolicy PP)
    {
        D.print(OS, PP);
    };

    template <class T>
    concept HasPrintWithPolicyFirst = requires(T const& D, llvm::raw_svector_ostream& OS, clang::PrintingPolicy PP)
    {
        D.print(PP, OS, true);
    };

    template <class T>
    concept HasDump = requires(T const& D, llvm::raw_svector_ostream& OS, clang::ASTContext const& C)
    {
        D.dump(OS, C);
    };

    template <class T>
    concept ConvertibleToUnqualifiedQualType = requires(T const& D)
    {
        QualType(&D, 0);
    };

    template <class T>
    concept HasGetName = requires(T const& D)
    {
        D.getName();
    };

    template <class T>
    requires (!std::is_pointer_v<T>)
    void
    printTraceName(T const& D, clang::ASTContext const& C, std::string& symbol_name);

    template <class T>
    void
    printTraceName(T const* D, clang::ASTContext const& C, std::string& symbol_name)
    {
        if (!D)
        {
            return;
        }
        llvm::raw_string_ostream os(symbol_name);
        if constexpr (std::derived_from<T, clang::Decl>)
        {
            if (clang::NamedDecl const* ND = dyn_cast<clang::NamedDecl>(D))
            {
                getQualifiedName(ND, os, C.getPrintingPolicy());
            }
            else
            {
                os << "<unnamed " << D->Decl::getDeclKindName() << ">";
            }
        }
        else if constexpr (HasPrintQualifiedName<T>)
        {
            D->printQualifiedName(os, C.getPrintingPolicy());
        }
        else if constexpr (HasPrint<T>)
        {
            D->print(os, C.getPrintingPolicy());
        }
        else if constexpr (HasPrintWithPolicyFirst<T>)
        {
            D->print(C.getPrintingPolicy(), os, true);
        }
        else if constexpr (ConvertibleToUnqualifiedQualType<T>)
        {
            clang::QualType const QT(D, 0);
            QT.print(os, C.getPrintingPolicy());
        }
        else if constexpr (HasDump<T>)
        {
            D->dump(os, C);
        }
        else if constexpr (HasGetName<T>)
        {
            os << D->getName();
        }
        else if constexpr (std::ranges::range<T>)
        {
            bool first = true;
            os << "{";
            for (auto it = D->begin(); it != D->end(); ++it)
            {
                if (!first)
                {
                    os << ", ";
                }
                first = false;
                printTraceName(*it, C, symbol_name);
            }
            os << "}";
        }
    }

    template <class T>
    requires (!std::is_pointer_v<T>)
    void
    printTraceName(T const& D, clang::ASTContext const& C, std::string& symbol_name)
    {
        printTraceName(&D, C, symbol_name);
    }

    template <class T>
    void
    printTraceName(Optional<T> const& D, clang::ASTContext const& C, std::string& symbol_name)
    {
        if (D)
        {
            printTraceName(*D, C, symbol_name);
        }
        else
        {
            symbol_name += "<empty>";
        }
    }
} // namespace detail

#    define MRDOCS_SYMBOL_TRACE_MERGE_(a, b) a##b
#    define MRDOCS_SYMBOL_TRACE_LABEL_(a)    MRDOCS_SYMBOL_TRACE_MERGE_(symbol_name_, a)
#    define MRDOCS_SYMBOL_TRACE_UNIQUE_NAME  MRDOCS_SYMBOL_TRACE_LABEL_(__LINE__)
#define MRDOCS_SYMBOL_TRACE(D, C) \
    std::string MRDOCS_SYMBOL_TRACE_UNIQUE_NAME;         \
    detail::printTraceName(D, C, MRDOCS_SYMBOL_TRACE_UNIQUE_NAME); \
    report::trace("{}", MRDOCS_SYMBOL_TRACE_UNIQUE_NAME)
#endif

} // mrdocs

#endif
