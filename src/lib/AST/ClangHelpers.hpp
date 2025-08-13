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

#include <mrdocs/Metadata/SymbolID.hpp>
#include <mrdocs/Support/Error.hpp>
#include <lib/AST/InstantiatedFromVisitor.hpp>
#include <clang/AST/Expr.h>
#include <clang/Sema/Sema.h>
#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Support/TypeTraits.hpp>
#include <clang/AST/AST.h>
#include <clang/AST/DeclFriend.h>
#include <clang/AST/DeclOpenMP.h>
#include <type_traits>

namespace clang::mrdocs {

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
Expr const*
SubstituteConstraintExpressionWithoutSatisfaction(
    Sema &S,
    const Sema::TemplateCompareNewDeclInfo &DeclInfo,
    const Expr *ConstrExpr);

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

// Extract NamespaceInfo from NamespaceDecl or TranslationUnitDecl
template <>
struct InfoTypeFor<NamespaceDecl>
    : std::type_identity<NamespaceInfo> {};

template <>
struct InfoTypeFor<TranslationUnitDecl>
    : std::type_identity<NamespaceInfo> {};

// Extract RecordInfo from anything derived from CXXRecordDecl
// and ClassTemplateDecl. Decls derived from CXXRecordDecl
// include class specializations.
template <>
struct InfoTypeFor<CXXRecordDecl>
    : std::type_identity<RecordInfo> {};

template <>
struct InfoTypeFor<ClassTemplateSpecializationDecl>
    : std::type_identity<RecordInfo> {};

template <>
struct InfoTypeFor<ClassTemplatePartialSpecializationDecl>
    : std::type_identity<RecordInfo> {};

template <>
struct InfoTypeFor<ClassTemplateDecl>
    : std::type_identity<RecordInfo> {};

// Extract FunctionInfo from anything derived from FunctionDecl
template <>
struct InfoTypeFor<FunctionDecl>
    : std::type_identity<FunctionInfo> {};

template <>
struct InfoTypeFor<CXXMethodDecl>
    : std::type_identity<FunctionInfo> {};

template <>
struct InfoTypeFor<CXXConstructorDecl>
    : std::type_identity<FunctionInfo> {};

template <>
struct InfoTypeFor<CXXDestructorDecl>
    : std::type_identity<FunctionInfo> {};

template <>
struct InfoTypeFor<CXXConversionDecl>
    : std::type_identity<FunctionInfo> {};

template <>
struct InfoTypeFor<FunctionTemplateDecl>
    : std::type_identity<FunctionInfo> {};

// Extract EnumInfo from EnumDecl
template <>
struct InfoTypeFor<EnumDecl>
    : std::type_identity<EnumInfo> {};

// Extract EnumConstantInfo from EnumConstantDecl
template <>
struct InfoTypeFor<EnumConstantDecl>
    : std::type_identity<EnumConstantInfo> {};

// Extract TypedefInfo from anything derived from TypedefNameDecl
template <>
struct InfoTypeFor<TypedefDecl>
    : std::type_identity<TypedefInfo> {};

template <>
struct InfoTypeFor<TypeAliasDecl>
    : std::type_identity<TypedefInfo> {};

template <>
struct InfoTypeFor<TypedefNameDecl>
    : std::type_identity<TypedefInfo> {};

template <>
struct InfoTypeFor<TypeAliasTemplateDecl>
    : std::type_identity<TypedefInfo> {};

// Extract VariableInfo from anything derived from VarDecl
// and VarTemplateDecl.
template <>
struct InfoTypeFor<VarDecl>
    : std::type_identity<VariableInfo> {};

template <>
struct InfoTypeFor<VarTemplateSpecializationDecl>
    : std::type_identity<VariableInfo> {};

template <>
struct InfoTypeFor<VarTemplatePartialSpecializationDecl>
    : std::type_identity<VariableInfo> {};

template <>
struct InfoTypeFor<VarTemplateDecl>
    : std::type_identity<VariableInfo> {};

template <>
struct InfoTypeFor<FieldDecl>
    : std::type_identity<VariableInfo> {};

// Extract GuideInfo from CXXDeductionGuideDecl
template <>
struct InfoTypeFor<CXXDeductionGuideDecl>
    : std::type_identity<GuideInfo> {};

// Extract NamespaceAliasInfo from NamespaceAliasDecl
template <>
struct InfoTypeFor<NamespaceAliasDecl>
    : std::type_identity<NamespaceAliasInfo> {};

// Extract UsingInfo from UsingDecl
template <>
struct InfoTypeFor<UsingDecl>
    : std::type_identity<UsingInfo> {};

// Extract ConceptInfo from ConceptDecl
template <>
struct InfoTypeFor<ConceptDecl>
    : std::type_identity<ConceptInfo> {};

/// Determine if there's a MrDocs Info type for a Clang DeclType
template <class T>
concept HasInfoTypeFor = std::derived_from<T, Decl> && requires
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
toAccessKind(AccessSpecifier const spec)
{
    switch(spec)
    {
    case AccessSpecifier::AS_public:    return AccessKind::Public;
    case AccessSpecifier::AS_protected: return AccessKind::Protected;
    case AccessSpecifier::AS_private:   return AccessKind::Private;
    case AccessSpecifier::AS_none:      return AccessKind::None;
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Convert a Clang StorageClass into a MrDocs StorageClassKind
 */
inline
StorageClassKind
toStorageClassKind(StorageClass const spec)
{
    switch(spec)
    {
    case StorageClass::SC_None:     return StorageClassKind::None;
    case StorageClass::SC_Extern:   return StorageClassKind::Extern;
    case StorageClass::SC_Static:   return StorageClassKind::Static;
    case StorageClass::SC_Auto:     return StorageClassKind::Auto;
    case StorageClass::SC_Register: return StorageClassKind::Register;
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
toConstexprKind(ConstexprSpecKind const spec)
{
    switch(spec)
    {
    case ConstexprSpecKind::Unspecified: return ConstexprKind::None;
    case ConstexprSpecKind::Constexpr:   return ConstexprKind::Constexpr;
    case ConstexprSpecKind::Consteval:   return ConstexprKind::Consteval;
    // KRYSTIAN NOTE: ConstexprSpecKind::Constinit exists,
    // but I don't think it's ever used because a variable
    // can be declared both constexpr and constinit
    // (but not both in the same declaration)
    case ConstexprSpecKind::Constinit:
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Convert a Clang ExplicitSpecKind into a MrDocs ExplicitKind
 */
inline
ExplicitKind
toExplicitKind(ExplicitSpecifier const& spec)
{
    // no explicit-specifier
    if (!spec.isSpecified())
    {
        return ExplicitKind::False;
    }

    switch(spec.getKind())
    {
    case ExplicitSpecKind::ResolvedFalse:  return ExplicitKind::False;
    case ExplicitSpecKind::ResolvedTrue:   return ExplicitKind::True;
    case ExplicitSpecKind::Unresolved:     return ExplicitKind::Dependent;
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Convert a Clang ExceptionSpecificationType into a MrDocs NoexceptKind
 */
inline
NoexceptKind
toNoexceptKind(ExceptionSpecificationType const spec)
{
    // KRYSTIAN TODO: right now we convert pre-C++17 dynamic exception
    // specifications to an (roughly) equivalent noexcept-specifier
    switch(spec)
    {
    case ExceptionSpecificationType::EST_None:
    case ExceptionSpecificationType::EST_MSAny:
    case ExceptionSpecificationType::EST_Unevaluated:
    case ExceptionSpecificationType::EST_Uninstantiated:
    // we *shouldn't* ever encounter an unparsed exception specification,
    // assuming that clang is working correctly...
    case ExceptionSpecificationType::EST_Unparsed:
    case ExceptionSpecificationType::EST_Dynamic:
    case ExceptionSpecificationType::EST_NoexceptFalse:
        return NoexceptKind::False;
    case ExceptionSpecificationType::EST_NoThrow:
    case ExceptionSpecificationType::EST_BasicNoexcept:
    case ExceptionSpecificationType::EST_NoexceptTrue:
    case ExceptionSpecificationType::EST_DynamicNone:
        return NoexceptKind::True;
    case ExceptionSpecificationType::EST_DependentNoexcept:
        return NoexceptKind::Dependent;
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Convert a Clang OverloadedOperatorKind into a MrDocs OperatorKind
 */
inline
OperatorKind
toOperatorKind(OverloadedOperatorKind const kind)
{
    switch(kind)
    {
    case OverloadedOperatorKind::OO_None:
        return OperatorKind::None;
    case OverloadedOperatorKind::OO_New:
        return OperatorKind::New;
    case OverloadedOperatorKind::OO_Delete:
        return OperatorKind::Delete;
    case OverloadedOperatorKind::OO_Array_New:
        return OperatorKind::ArrayNew;
    case OverloadedOperatorKind::OO_Array_Delete:
        return OperatorKind::ArrayDelete;
    case OverloadedOperatorKind::OO_Plus:
        return OperatorKind::Plus;
    case OverloadedOperatorKind::OO_Minus:
        return OperatorKind::Minus;
    case OverloadedOperatorKind::OO_Star:
        return OperatorKind::Star;
    case OverloadedOperatorKind::OO_Slash:
        return OperatorKind::Slash;
    case OverloadedOperatorKind::OO_Percent:
        return OperatorKind::Percent;
    case OverloadedOperatorKind::OO_Caret:
        return OperatorKind::Caret;
    case OverloadedOperatorKind::OO_Amp:
        return OperatorKind::Amp;
    case OverloadedOperatorKind::OO_Pipe:
        return OperatorKind::Pipe;
    case OverloadedOperatorKind::OO_Tilde:
        return OperatorKind::Tilde;
    case OverloadedOperatorKind::OO_Exclaim:
        return OperatorKind::Exclaim;
    case OverloadedOperatorKind::OO_Equal:
        return OperatorKind::Equal;
    case OverloadedOperatorKind::OO_Less:
        return OperatorKind::Less;
    case OverloadedOperatorKind::OO_Greater:
        return OperatorKind::Greater;
    case OverloadedOperatorKind::OO_PlusEqual:
        return OperatorKind::PlusEqual;
    case OverloadedOperatorKind::OO_MinusEqual:
        return OperatorKind::MinusEqual;
    case OverloadedOperatorKind::OO_StarEqual:
        return OperatorKind::StarEqual;
    case OverloadedOperatorKind::OO_SlashEqual:
        return OperatorKind::SlashEqual;
    case OverloadedOperatorKind::OO_PercentEqual:
        return OperatorKind::PercentEqual;
    case OverloadedOperatorKind::OO_CaretEqual:
        return OperatorKind::CaretEqual;
    case OverloadedOperatorKind::OO_AmpEqual:
        return OperatorKind::AmpEqual;
    case OverloadedOperatorKind::OO_PipeEqual:
        return OperatorKind::PipeEqual;
    case OverloadedOperatorKind::OO_LessLess:
        return OperatorKind::LessLess;
    case OverloadedOperatorKind::OO_GreaterGreater:
        return OperatorKind::GreaterGreater;
    case OverloadedOperatorKind::OO_LessLessEqual:
        return OperatorKind::LessLessEqual;
    case OverloadedOperatorKind::OO_GreaterGreaterEqual:
        return OperatorKind::GreaterGreaterEqual;
    case OverloadedOperatorKind::OO_EqualEqual:
        return OperatorKind::EqualEqual;
    case OverloadedOperatorKind::OO_ExclaimEqual:
        return OperatorKind::ExclaimEqual;
    case OverloadedOperatorKind::OO_LessEqual:
        return OperatorKind::LessEqual;
    case OverloadedOperatorKind::OO_GreaterEqual:
        return OperatorKind::GreaterEqual;
    case OverloadedOperatorKind::OO_Spaceship:
        return OperatorKind::Spaceship;
    case OverloadedOperatorKind::OO_AmpAmp:
        return OperatorKind::AmpAmp;
    case OverloadedOperatorKind::OO_PipePipe:
        return OperatorKind::PipePipe;
    case OverloadedOperatorKind::OO_PlusPlus:
        return OperatorKind::PlusPlus;
    case OverloadedOperatorKind::OO_MinusMinus:
        return OperatorKind::MinusMinus;
    case OverloadedOperatorKind::OO_Comma:
        return OperatorKind::Comma;
    case OverloadedOperatorKind::OO_ArrowStar:
        return OperatorKind::ArrowStar;
    case OverloadedOperatorKind::OO_Arrow:
        return OperatorKind::Arrow;
    case OverloadedOperatorKind::OO_Call:
        return OperatorKind::Call;
    case OverloadedOperatorKind::OO_Subscript:
        return OperatorKind::Subscript;
    case OverloadedOperatorKind::OO_Conditional:
        return OperatorKind::Conditional;
    case OverloadedOperatorKind::OO_Coawait:
        return OperatorKind::Coawait;
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Convert a Clang ReferenceKind into a MrDocs ReferenceKind
 */
inline
ReferenceKind
toReferenceKind(RefQualifierKind const kind)
{
    switch(kind)
    {
    case RefQualifierKind::RQ_None:
        return ReferenceKind::None;
    case RefQualifierKind::RQ_LValue:
        return ReferenceKind::LValue;
    case RefQualifierKind::RQ_RValue:
        return ReferenceKind::RValue;
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Convert a Clang TagTypeKind into a MrDocs RecordKeyKind
 */
inline
RecordKeyKind
toRecordKeyKind(TagTypeKind const kind)
{
    switch(kind)
    {
    case TagTypeKind::Struct: return RecordKeyKind::Struct;
    case TagTypeKind::Class:  return RecordKeyKind::Class;
    case TagTypeKind::Union:  return RecordKeyKind::Union;
    default:
        // unsupported TagTypeKind (Interface, or Enum)
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
    if (quals & Qualifiers::Const)
    {
        result |= QualifierKind::Const;
    }
    if (quals & Qualifiers::Volatile)
    {
        result |= QualifierKind::Volatile;
    }
    return static_cast<QualifierKind>(result);

}

/** Convert a Clang Decl::Kind into a MrDocs FunctionClass
 */
inline
FunctionClass
toFunctionClass(Decl::Kind const kind)
{
    switch(kind)
    {
    case Decl::Kind::Function:
    case Decl::Kind::CXXMethod:         return FunctionClass::Normal;
    case Decl::Kind::CXXConstructor:    return FunctionClass::Constructor;
    case Decl::Kind::CXXConversion:     return FunctionClass::Conversion;
    case Decl::Kind::CXXDestructor:     return FunctionClass::Destructor;
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Convert a Clang AutoTypeKeyword into a MrDocs AutoKind
 */
inline
AutoKind
toAutoKind(AutoTypeKeyword const kind)
{
    switch(kind)
    {
    case AutoTypeKeyword::Auto:
    case AutoTypeKeyword::GNUAutoType:
      return AutoKind::Auto;
    case AutoTypeKeyword::DecltypeAuto:
      return AutoKind::DecltypeAuto;
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Convert a Clang AutoTypeKeyword into a MrDocs AutoKind
 */
inline
std::optional<FundamentalTypeKind>
toFundamentalTypeKind(BuiltinType::Kind const kind)
{
    switch(kind)
    {
    case BuiltinType::Kind::Void:
        return FundamentalTypeKind::Void;
    case BuiltinType::Kind::NullPtr:
        return FundamentalTypeKind::Nullptr;
    case BuiltinType::Kind::Bool:
        return FundamentalTypeKind::Bool;
    case BuiltinType::Kind::Char_U:
    case BuiltinType::Kind::Char_S:
        return FundamentalTypeKind::Char;
    case BuiltinType::Kind::SChar:
        return FundamentalTypeKind::SignedChar;
    case BuiltinType::Kind::UChar:
        return FundamentalTypeKind::UnsignedChar;
    case BuiltinType::Kind::Char8:
        return FundamentalTypeKind::Char8;
    case BuiltinType::Kind::Char16:
        return FundamentalTypeKind::Char16;
    case BuiltinType::Kind::Char32:
        return FundamentalTypeKind::Char32;
    case BuiltinType::Kind::WChar_S:
    case BuiltinType::Kind::WChar_U:
        return FundamentalTypeKind::WChar;
    case BuiltinType::Kind::Short:
        return FundamentalTypeKind::Short;
    case BuiltinType::Kind::UShort:
        return FundamentalTypeKind::UnsignedShort;
    case BuiltinType::Kind::Int:
        return FundamentalTypeKind::Int;
    case BuiltinType::Kind::UInt:
        return FundamentalTypeKind::UnsignedInt;
    case BuiltinType::Kind::Long:
        return FundamentalTypeKind::Long;
    case BuiltinType::Kind::ULong:
        return FundamentalTypeKind::UnsignedLong;
    case BuiltinType::Kind::LongLong:
        return FundamentalTypeKind::LongLong;
    case BuiltinType::Kind::ULongLong:
        return FundamentalTypeKind::UnsignedLongLong;
    case BuiltinType::Kind::Float:
        return FundamentalTypeKind::Float;
    case BuiltinType::Kind::Double:
        return FundamentalTypeKind::Double;
    case BuiltinType::Kind::LongDouble:
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
    Decl const* resultDecl = InstantiatedFromVisitor().Visit(D);
    return cast<DeclTy>(resultDecl);
}

template <class DeclTy>
requires
    std::derived_from<DeclTy, FunctionDecl> ||
    std::same_as<FunctionTemplateDecl, std::remove_cv_t<DeclTy>>
FunctionDecl const*
getInstantiatedFrom(DeclTy const* D)
{
    return dyn_cast_if_present<FunctionDecl>(
        getInstantiatedFrom<Decl>(D));
}

template <class DeclTy>
requires
    std::derived_from<DeclTy, CXXRecordDecl> ||
    std::same_as<ClassTemplateDecl, std::remove_cv_t<DeclTy>>
CXXRecordDecl const*
getInstantiatedFrom(DeclTy const* D)
{
    return dyn_cast_if_present<CXXRecordDecl>(
        getInstantiatedFrom<Decl>(D));
}

template <class DeclTy>
requires
    std::derived_from<DeclTy, VarDecl> ||
    std::same_as<VarTemplateDecl, std::remove_cv_t<DeclTy>>
VarDecl const*
getInstantiatedFrom(DeclTy const* D)
{
    return dyn_cast_if_present<VarDecl>(
        getInstantiatedFrom<Decl>(D));
}

template <class DeclTy>
requires
    std::derived_from<DeclTy, TypedefNameDecl> ||
    std::same_as<TypeAliasTemplateDecl, std::remove_cv_t<DeclTy>>
TypedefNameDecl const*
getInstantiatedFrom(DeclTy const* D)
{
    return dyn_cast_if_present<TypedefNameDecl>(
        getInstantiatedFrom<Decl>(D));
}

/** Get the access specifier for a `Decl`

    Given a `Decl`, this function will analyze the parent
    context and return the access specifier for the declaration.
 */
MRDOCS_DECL
AccessSpecifier
getAccess(Decl const* D);

MRDOCS_DECL
QualType
getDeclaratorType(DeclaratorDecl const* DD);

/** Get the NonTypeTemplateParm of an expression

    This function will return the NonTypeTemplateParmDecl
    corresponding to the expression `E` if it is a
    NonTypeTemplateParmDecl. If the expression is not
    a NonTypeTemplateParmDecl, the function will return
    nullptr.

    For instance, given the expression `x` in the following
    code snippet:

    @code
    template<int x>
    void f() {}
    @endcode

    the function will return the NonTypeTemplateParmDecl
    corresponding to `x`, which is the template parameter
    of the function `f`.
 */
MRDOCS_DECL
NonTypeTemplateParmDecl const*
getNTTPFromExpr(Expr const* E, unsigned Depth);

// Get the parent declaration of a declaration
MRDOCS_DECL
Decl const*
getParent(Decl const* D);

MRDOCS_DECL
void
getQualifiedName(
    NamedDecl const* ND,
    raw_ostream &Out,
    const PrintingPolicy &Policy);

// If D refers to an implicit instantiation of a template specialization,
// decay it to the Decl of the primary template. The template arguments
// will be extracted separately as part of the TypeInfo.
// For instance, a Decl to `S<0>` becomes a Decl to `S`, unless `S<0>` is
// an explicit specialization of the primary template.
// This function also applies recursively to the parent of D so that
// the primary template is resolved for nested classes.
// For instance, a Decl to `A<0>::S` becomes a Decl to `A::S`, unless
// `A<0>` is an explicit specialization of the primary template.
MRDOCS_DECL
Decl const*
decayToPrimaryTemplate(Decl const* D);

// Iterate the Decl and check if this is a template specialization
// also considering the parent declarations. For instance,
// S<0>::M<0> is a template specialization of S<0> and M<0>.
// This function returns true if both S<0> and M<0> are implicit
// template specializations.
MRDOCS_DECL
bool
isAllImplicitSpecialization(Decl const* D);

// Check if any component of D is an implicit specialization
MRDOCS_DECL
bool
isAnyImplicitSpecialization(Decl const* D);

// Check if at least one component of D is explicit
inline
bool
isAnyExplicitSpecialization(Decl const* D)
{
    return !isAllImplicitSpecialization(D);
}

// Check if all components are explicit
inline
bool
isAllExplicitSpecialization(Decl const* D)
{
    return !isAnyImplicitSpecialization(D);
}

MRDOCS_DECL
bool
isVirtualMember(Decl const* D);

MRDOCS_DECL
bool
isAnonymousNamespace(Decl const* D);

MRDOCS_DECL
bool
isStaticFileLevelMember(Decl const *D);

MRDOCS_DECL
bool
isDocumented(Decl const *D);

MRDOCS_DECL
RawComment const*
getDocumentation(Decl const *D);

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
    concept HasPrintQualifiedName = requires(T const& D, llvm::raw_svector_ostream& OS, PrintingPolicy PP)
    {
        D.printQualifiedName(OS, PP);
    };

    template <class T>
    concept HasPrint = requires(T const& D, llvm::raw_svector_ostream& OS, PrintingPolicy PP)
    {
        D.print(OS, PP);
    };

    template <class T>
    concept HasPrintWithPolicyFirst = requires(T const& D, llvm::raw_svector_ostream& OS, PrintingPolicy PP)
    {
        D.print(PP, OS, true);
    };

    template <class T>
    concept HasDump = requires(T const& D, llvm::raw_svector_ostream& OS, ASTContext const& C)
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
    printTraceName(T const& D, ASTContext const& C, std::string& symbol_name);

    template <class T>
    void
    printTraceName(T const* D, ASTContext const& C, std::string& symbol_name)
    {
        if (!D)
        {
            return;
        }
        llvm::raw_string_ostream os(symbol_name);
        if constexpr (std::derived_from<T, Decl>)
        {
            if (NamedDecl const* ND = dyn_cast<NamedDecl>(D))
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
            QualType const QT(D, 0);
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
    printTraceName(T const& D, ASTContext const& C, std::string& symbol_name)
    {
        printTraceName(&D, C, symbol_name);
    }

    template <class T>
    void
    printTraceName(std::optional<T> const& D, ASTContext const& C, std::string& symbol_name)
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

} // clang::mrdocs

#endif
