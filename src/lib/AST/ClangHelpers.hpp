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

#include <mrdocs/Metadata/Symbols.hpp>
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
const Expr*
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
template <std::derived_from<CXXRecordDecl> DeclType>
struct InfoTypeFor<DeclType>
    : std::type_identity<RecordInfo> {};

// Extract FunctionInfo from anything derived from FunctionDecl
template <std::derived_from<FunctionDecl> FunctionTy>
struct InfoTypeFor<FunctionTy>
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
template <std::derived_from<TypedefNameDecl> TypedefNameTy>
struct InfoTypeFor<TypedefNameTy>
    : std::type_identity<TypedefInfo> {};

// Extract VariableInfo from anything derived from VarDecl
template <std::derived_from<VarDecl> VarTy>
struct InfoTypeFor<VarTy>
    : std::type_identity<VariableInfo> {};

// Extract FieldInfo from FieldDecl
template <>
struct InfoTypeFor<FieldDecl>
    : std::type_identity<FieldInfo> {};

// Extract FriendInfo from FriendDecl
template <>
struct InfoTypeFor<FriendDecl>
    : std::type_identity<FriendInfo> {};

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
concept HasInfoTypeFor = requires
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
convertToAccessKind(AccessSpecifier const spec)
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
convertToExplicitKind(ExplicitSpecifier const& spec)
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
convertToNoexceptKind(ExceptionSpecificationType const spec)
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
convertToOperatorKind(OverloadedOperatorKind const kind)
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
convertToReferenceKind(RefQualifierKind const kind)
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
convertToQualifierKind(unsigned const quals)
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
convertToAutoKind(AutoTypeKeyword const kind)
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
    user-written `Decl` corresponding to `D`. For specializations
    which were implicitly instantiated, this will be whichever `Decl`
    was used as the pattern for instantiation.
*/
template <class DeclTy>
DeclTy*
getInstantiatedFrom(DeclTy* D)
{
    if (!D)
    {
        return nullptr;
    }
    auto* decayedD = const_cast<Decl*>(static_cast<const Decl*>(D));
    Decl* resultDecl = InstantiatedFromVisitor().Visit(decayedD);
    return cast<DeclTy>(resultDecl);
}

template <class DeclTy>
requires
    std::derived_from<DeclTy, FunctionDecl> ||
    std::same_as<FunctionTemplateDecl, std::remove_cv_t<DeclTy>>
FunctionDecl*
getInstantiatedFrom(DeclTy* D)
{
    return dyn_cast_if_present<FunctionDecl>(
        getInstantiatedFrom<Decl>(D));
}

template <class DeclTy>
requires
    std::derived_from<DeclTy, CXXRecordDecl> ||
    std::same_as<ClassTemplateDecl, std::remove_cv_t<DeclTy>>
CXXRecordDecl*
getInstantiatedFrom(DeclTy* D)
{
    return dyn_cast_if_present<CXXRecordDecl>(
        getInstantiatedFrom<Decl>(D));
}

template <class DeclTy>
requires
    std::derived_from<DeclTy, VarDecl> ||
    std::same_as<VarTemplateDecl, std::remove_cv_t<DeclTy>>
VarDecl*
getInstantiatedFrom(DeclTy* D)
{
    return dyn_cast_if_present<VarDecl>(
        getInstantiatedFrom<Decl>(D));
}

template <class DeclTy>
requires
    std::derived_from<DeclTy, TypedefNameDecl> ||
    std::same_as<TypeAliasTemplateDecl, std::remove_cv_t<DeclTy>>
TypedefNameDecl*
getInstantiatedFrom(DeclTy* D)
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
getAccess(const Decl* D);

MRDOCS_DECL
QualType
getDeclaratorType(const DeclaratorDecl* DD);

MRDOCS_DECL
NonTypeTemplateParmDecl const*
getNTTPFromExpr(const Expr* E, unsigned Depth);

// Get the parent declaration of a declaration
MRDOCS_DECL
Decl*
getParent(Decl* D);

// Get the parent declaration of a declaration
inline
Decl const*
getParent(Decl const* D) {
    return getParent(const_cast<Decl*>(D));
}


} // clang::mrdocs

#endif
