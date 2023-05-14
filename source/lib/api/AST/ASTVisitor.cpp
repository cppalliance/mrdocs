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

#include "ASTVisitor.hpp"
#include "Bitcode.hpp"
#include "Commands.hpp"
#include "api/ConfigImpl.hpp"
#include "ParseJavadoc.hpp"
#include "api/Support/Path.hpp"
#include "api/Support/Debug.hpp"
#include <mrdox/Metadata.hpp>
#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclFriend.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Index/USRGeneration.h>
#include <clang/Lex/Lexer.h>
#include <llvm/ADT/Hashing.h>
#include <llvm/ADT/Optional.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/SHA1.h>
#include <cassert>

namespace clang {
namespace mrdox {

//------------------------------------------------
//
// ASTVisitor
//
//------------------------------------------------

ASTVisitor::
ASTVisitor(
    tooling::ExecutionContext& ex,
    ConfigImpl const& config,
    Reporter& R) noexcept
    : ex_(ex)
    , config_(config)
    , R_(R)
    , PublicOnly(! config_.includePrivate)
    , IsFileInRootDir(true)
{
}

//------------------------------------------------

// Function to hash a given USR value for storage.
// As USRs (Unified Symbol Resolution) could be
// large, especially for functions with long type
// arguments, we use 160-bits SHA1(USR) values to
// guarantee the uniqueness of symbols while using
// a relatively small amount of memory (vs storing
// USRs directly).
//
static
SymbolID
getUSRForDecl(
    Decl const* D)
{
    llvm::SmallString<128> USR;
    if(index::generateUSRForDecl(D, USR))
        return SymbolID();
    return llvm::SHA1::hash(arrayRefFromStringRef(USR));
}

//------------------------------------------------

static
bool
shouldSerializeInfo(
    bool PublicOnly,
    bool IsInAnonymousNamespace,
    NamedDecl const* D) noexcept
{
    if(! PublicOnly)
        return true;
    if(IsInAnonymousNamespace)
        return false;
    if(auto const* N = dyn_cast<NamespaceDecl>(D))
        if(N->isAnonymousNamespace())
            return false;
    // bool isPublic()
    AccessSpecifier access = D->getAccessUnsafe();
    if(access == AccessSpecifier::AS_private)
        return false;
    Linkage linkage = D->getLinkageInternal();
    if( linkage == Linkage::ModuleLinkage ||
        linkage == Linkage::ExternalLinkage)
        return true;
    // some form of internal linkage
    return false; 
}

//------------------------------------------------

static
void
getParent(
    SymbolID& parent,
    Decl const* D)
{
    bool isParentAnonymous = false;
    DeclContext const* DC = D->getDeclContext();
    Assert(DC != nullptr);
    if(auto const* N = dyn_cast<NamespaceDecl>(DC))
    {
        if(N->isAnonymousNamespace())
        {
            isParentAnonymous = true;
        }
        parent = getUSRForDecl(N);
    }
    else if(auto const* N = dyn_cast<RecordDecl>(DC))
    {
        parent = getUSRForDecl(N);
    }
    else if(auto const* N = dyn_cast<FunctionDecl>(DC))
    {
        parent = getUSRForDecl(N);
    }
    else if(auto const* N = dyn_cast<EnumDecl>(DC))
    {
        parent = getUSRForDecl(N);
    }
    else
    {
        Assert(false);
    }
    (void)isParentAnonymous;
}

static
void
getParentNamespaces(
    llvm::SmallVector<Reference, 4>& Namespaces,
    Decl const* D,
    bool& IsInAnonymousNamespace)
{
    IsInAnonymousNamespace = false;
    DeclContext const* DC = D->getDeclContext();
    do
    {
        if(auto const* N = dyn_cast<NamespaceDecl>(DC))
        {
            std::string Namespace;
            if(N->isAnonymousNamespace())
            {
                Namespace = "@nonymous_namespace";
                IsInAnonymousNamespace = true;
            }
            else
            {
                Namespace = N->getNameAsString();
            }
            Namespaces.emplace_back(
                getUSRForDecl(N),
                Namespace,
                InfoType::IT_namespace);
        }
        else if(auto const* N = dyn_cast<RecordDecl>(DC))
        {
            Namespaces.emplace_back(
                getUSRForDecl(N),
                N->getNameAsString(),
                InfoType::IT_record);
        }
        else if(auto const* N = dyn_cast<FunctionDecl>(DC))
        {
            Namespaces.emplace_back(
                getUSRForDecl(N),
                N->getNameAsString(),
                InfoType::IT_function);
        }
        else if(auto const* N = dyn_cast<EnumDecl>(DC))
        {
            Namespaces.emplace_back(
                getUSRForDecl(N),
                N->getNameAsString(),
                InfoType::IT_enum);
        }
    }
    while((DC = DC->getParent()));

    // The global namespace should be added to the
    // list of namespaces if the decl corresponds to
    // a Record and if it doesn't have any namespace
    // (because this means it's in the global namespace).
    // Also if its outermost namespace is a record because
    // that record matches the previous condition mentioned.
    if((Namespaces.empty() && isa<RecordDecl>(D)) ||
        (!Namespaces.empty() && Namespaces.back().RefType == InfoType::IT_record))
    {
        Namespaces.emplace_back(
            globalNamespaceID,
            "", //"GlobalNamespace",
            InfoType::IT_namespace);
    }
}

//------------------------------------------------

static
std::string
getSourceCode(
    Decl const* D,
    SourceRange const& R)
{
    return Lexer::getSourceText(CharSourceRange::getTokenRange(R),
        D->getASTContext().getSourceManager(),
        D->getASTContext().getLangOpts())
        .str();
}

//------------------------------------------------

static
Access
getAccessFromSpecifier(
    AccessSpecifier as) noexcept
{
    switch(as)
    {
    case AccessSpecifier::AS_public:
        return Access::Public;
    case AccessSpecifier::AS_protected:
        return Access::Protected;
    case AccessSpecifier::AS_private:
        return Access::Private;
    default:
        llvm_unreachable("unknown AccessSpecifier");
    }
}

static
TagDecl*
getTagDeclForType(
    QualType const& T)
{
    if(TagDecl const* D = T->getAsTagDecl())
        return D->getDefinition();
    return nullptr;
}

static
RecordDecl*
getRecordDeclForType(
    QualType const& T)
{
    if(RecordDecl const* D = T->getAsRecordDecl())
        return D->getDefinition();
    return nullptr;
}

static
TypeInfo
getTypeInfoForType(
    QualType const& T)
{
    TagDecl const* TD = getTagDeclForType(T);
    if (T->isBuiltinType()
    && (T->getAs<clang::BuiltinType>()->getKind() == BuiltinType::Bool))
        return TypeInfo(Reference(EmptySID, "bool"));
    if(!TD)
        return TypeInfo(Reference(EmptySID, T.getAsString()));
    InfoType IT;
    if(dyn_cast<EnumDecl>(TD))
        IT = InfoType::IT_enum;
    else if(dyn_cast<RecordDecl>(TD))
        IT = InfoType::IT_record;
    else
        IT = InfoType::IT_default;
    return TypeInfo(Reference(
        getUSRForDecl(TD), TD->getNameAsString(), IT));
}

static
void
parseParameters(
    FunctionInfo& I,
    FunctionDecl const* D)
{
    for(ParmVarDecl const* P : D->parameters())
    {
        // KRYSTIAN NOTE: call getOriginalType instead
        // of getType if we want to preserve top-level
        // cv-qualfiers/array types/function types
        auto ti = getTypeInfoForType(P->getType());

        FieldTypeInfo& FieldInfo = I.Params.emplace_back(
            getTypeInfoForType(P->getType()),
            P->getNameAsString());
        FieldInfo.DefaultValue = getSourceCode(
            D, P->getDefaultArgRange());
    }
}

void
getTemplateParams(
    llvm::Optional<TemplateInfo>& TemplateInfo,
    const Decl* D)
{
    if(TemplateParameterList const* ParamList =
        D->getDescribedTemplateParams())
    {
        if(!TemplateInfo)
        {
            TemplateInfo.emplace();
        }
        for(const NamedDecl* ND : *ParamList)
        {
            TemplateInfo->Params.emplace_back(*ND);
        }
    }
}

static
void
parseJavadoc(
    llvm::Optional<Javadoc>& javadoc,
    Decl const* D,
    Reporter& R)
{
    // VFALCO investigate whether we can use
    // ASTContext::getCommentForDecl instead
    RawComment* RC =
        D->getASTContext().getRawCommentForDeclNoCache(D);
    if(RC)
    {
        RC->setAttached();
        javadoc.emplace(parseJavadoc(RC, D, R));
    }
    else
    {
        javadoc.reset();
    }
}

//------------------------------------------------

static
void
getMemberTypeInfo(
    MemberTypeInfo& I,
    FieldDecl const* D,
    Reporter& R)
{
    Assert(D && "Expect non-null FieldDecl in getMemberTypeInfo");
    parseJavadoc(I.javadoc, D, R);

    for (auto attr : D->attrs())
    {
        switch (attr->getKind())
        {
            case clang::attr::Kind::NoUniqueAddress:
                I.Flags.hasNoUniqueAddress = true;
                break;
            case clang::attr::Kind::Deprecated:
                I.Flags.isDeprecated = true;
                break;
            case clang::attr::Kind::Unused:
                I.Flags.isNodiscard = true;
                break;
            default:
                break;
        }
    }
}


//------------------------------------------------

template<class Child>
static
void
insertChild(
    RecordInfo& P, Child const& I, Access access)
{
    if constexpr(std::is_same_v<Child, RecordInfo>)
    {
        P.Children_.Records.push_back({ I.id, access });
    }
    else if constexpr(std::is_same_v<Child, FunctionInfo>)
    {
        P.Children_.Functions.push_back({ I.id, access });
    }
    else if constexpr(std::is_same_v<Child, TypedefInfo>)
    {
        P.Children_.Types.push_back({ I.id, access });
    }
    else if constexpr(std::is_same_v<Child, EnumInfo>)
    {
        P.Children_.Enums.push_back({ I.id, access });
    }
    else if constexpr(std::is_same_v<Child, VarInfo>)
    {
        P.Children_.Vars.push_back({ I.id, access });
    }
    else
    {
        Assert(false);
    }
}

template<class Child>
requires std::derived_from<Child, Info>
static
void
insertChild(NamespaceInfo& parent, Child const& I)
{
    if constexpr(std::is_same_v<Child, NamespaceInfo>)
    {
        parent.Children.Namespaces.emplace_back(I.id, I.Name, Child::type_id);
    }
    else if constexpr(std::is_same_v<Child, RecordInfo>)
    {
        parent.Children.Records.emplace_back(I.id, I.Name, Child::type_id);
    }
    else if constexpr(std::is_same_v<Child, FunctionInfo>)
    {
        parent.Children.Functions.emplace_back(I.id, I.Name, Child::type_id);
    }
    else if constexpr(std::is_same_v<Child, TypedefInfo>)
    {
        parent.Children.Typedefs.emplace_back(I.id, I.Name, Child::type_id);
    }
    else if constexpr(std::is_same_v<Child, EnumInfo>)
    {
        parent.Children.Enums.emplace_back(I.id, I.Name, Child::type_id);
    }
    else if constexpr(std::is_same_v<Child, VarInfo>)
    {
        parent.Children.Vars.emplace_back(I.id, I.Name, Child::type_id);
    }
    else
    {
        Assert(false);
    }
}

template<class Child>
requires std::derived_from<Child, Info>
static
Bitcode
writeParent(
    Child const& I,
    AccessSpecifier access = AccessSpecifier::AS_none)
{
    Access access_;
    switch(access)
    {
    case AccessSpecifier::AS_none:
    {
        // Create an empty parent for the child with the
        // child inserted either as a reference or by moving
        // the entire record. Then return the parent as a
        // serialized bitcode.
        if(I.Namespace.empty())
        {
            if(I.id == globalNamespaceID)
            {
                // Global namespace has no parent.
                return {};
            }

            // In global namespace
            NamespaceInfo P;
            Assert(P.id == globalNamespaceID);
            insertChild(P, I);
            return writeBitcode(P);
        }
        Assert(I.Namespace[0].RefType == InfoType::IT_namespace);
        NamespaceInfo P(I.Namespace[0].id);
        insertChild(P, I);
        return writeBitcode(P);
    }
    case AccessSpecifier::AS_public:
        access_ = Access::Public;
        break;
    case AccessSpecifier::AS_protected:
        access_ = Access::Protected;
        break;
    case AccessSpecifier::AS_private:
        access_ = Access::Private;
        break;
    default:
        llvm_unreachable("unknown access");
    }

    // Create an empty Record for the child,
    // and insert the child as a RefWithAccess.
    // Then return the parent as a serialized bitcode.
    Assert(! I.Namespace.empty());
    Assert(I.Namespace[0].RefType == InfoType::IT_record);
    Assert(Child::type_id != InfoType::IT_namespace);
    RecordInfo P(I.Namespace[0].id);
    insertChild(P, I, access_);
    return writeBitcode(P);
}

static
void
parseFields(
    RecordInfo& I,
    const RecordDecl* D,
    bool PublicOnly,
    AccessSpecifier Access,
    Reporter& R)
{
    for(const FieldDecl* F : D->fields())
    {
        if(!shouldSerializeInfo(PublicOnly, /*IsInAnonymousNamespace=*/false, F))
            continue;

        // Use getAccessUnsafe so that we just get the default AS_none if it's not
        // valid, as opposed to an assert.
        MemberTypeInfo& NewMember = I.Members.emplace_back(
            getTypeInfoForType(F->getTypeSourceInfo()->getType()),
            F->getNameAsString(),
            getAccessFromSpecifier(F->getAccessUnsafe()));
        getMemberTypeInfo(NewMember, F, R);
    }
}

static
void
parseEnumerators(
    EnumInfo& I,
    const EnumDecl* D)
{
    for(const EnumConstantDecl* E : D->enumerators())
    {
        std::string ValueExpr;
        if(const Expr* InitExpr = E->getInitExpr())
            ValueExpr = getSourceCode(D, InitExpr->getSourceRange());

        SmallString<16> ValueStr;
        E->getInitVal().toString(ValueStr);
        I.Members.emplace_back(E->getNameAsString(), ValueStr, ValueExpr);
    }
}

//------------------------------------------------

// This also sets IsFileInRootDir
bool
ASTVisitor::
shouldExtract(
    Decl const* D)
{
    namespace path = llvm::sys::path;

    if(sourceManager_->isInSystemHeader(D->getLocation()))
    {
        // skip system header
        return false;
    }

    if(D->getParentFunctionOrMethod())
    {
        // skip function-local declaration,
        // and skip function ParmVarDecls.
        return false;
    }

    clang::PresumedLoc const loc =
        sourceManager_->getPresumedLoc(D->getBeginLoc());
    auto result = fileFilter_.emplace(
        loc.getIncludeLoc().getRawEncoding(),
        FileFilter());
    if(! result.second)
    {
        // cached filter entry already exists
        FileFilter const& ff = result.first->second;
        if(! ff.include)
            return false;
        File = loc.getFilename(); // native
        convert_to_slash(File);
        // VFALCO we could assert that the prefix
        //        matches and just lop off the
        //        first ff.prefix.size() characters.
        path::replace_path_prefix(File, ff.prefix, "");
    }
    else
    {
        // new element
        File = loc.getFilename();
        convert_to_slash(File);
        FileFilter& ff = result.first->second;
        ff.include = config_.shouldVisitFile(File, ff.prefix);
        if(! ff.include)
            return false;
        // VFALCO we could assert that the prefix
        //        matches and just lop off the
        //        first ff.prefix.size() characters.
        path::replace_path_prefix(File, ff.prefix, "");
    }

    IsFileInRootDir = true;

    return true;
}

bool
ASTVisitor::
extractSymbolID(
    SymbolID& id, NamedDecl const* D)
{
    usr_.clear();
    auto const shouldIgnore = index::generateUSRForDecl(D, usr_);
    if(shouldIgnore)
        return false;
    id = llvm::SHA1::hash(arrayRefFromStringRef(usr_));
    return true;
}

bool
ASTVisitor::
extractInfo(
    Info& I,
    NamedDecl const* D)
{
    bool IsInAnonymousNamespace;
    getParentNamespaces(
        I.Namespace, D, IsInAnonymousNamespace);
    if(! shouldSerializeInfo(PublicOnly, IsInAnonymousNamespace, D))
        return false;
    if(! extractSymbolID(I.id, D))
        return false;
    I.Name = D->getNameAsString();
    parseJavadoc(I.javadoc, D, R_);
    return true;
}

// Return a Reference for the function.
Reference
ASTVisitor::
getFunctionReference(
    FunctionDecl const* D)
{
    Reference ref;
    /*shouldExtract=*/ extractSymbolID(ref.id, D);
    ref.Name = D->getNameAsString();
    ref.RefType = InfoType::IT_function;
    return ref;
}

int
ASTVisitor::
getLine(
    NamedDecl const* D) const
{
    return sourceManager_->getPresumedLoc(
        D->getBeginLoc()).getLine();
}

void
ASTVisitor::
extractBases(
    RecordInfo& I, CXXRecordDecl* D)
{
    // Base metadata is only available for definitions.
    if(! D->isThisDeclarationADefinition())
        return;

    // Only direct bases
    for(CXXBaseSpecifier const& B : D->bases())
    {
        auto const isVirtual = B.isVirtual();
        if(isVirtual && ! config_.includePrivate)
            continue;
        if(auto const* Ty = B.getType()->getAs<TemplateSpecializationType>())
        {
            TemplateDecl const* D = Ty->getTemplateName().getAsTemplateDecl();
            I.Bases.emplace_back(
                getUSRForDecl(D),
                B.getType().getAsString(),
                getAccessFromSpecifier(B.getAccessSpecifier()),
                isVirtual);
        }
        else if(RecordDecl const* P = getRecordDeclForType(B.getType()))
        {
            I.Bases.emplace_back(
                getUSRForDecl(P),
                P->getNameAsString(),
                getAccessFromSpecifier(B.getAccessSpecifier()),
                isVirtual);
        }
        else
        {
            I.Bases.emplace_back(
                EmptySID,
                B.getType().getAsString(),
                getAccessFromSpecifier(B.getAccessSpecifier()),
                isVirtual);
        }
    }
}

//------------------------------------------------

template<class DeclTy>
bool
ASTVisitor::
constructFunction(
    FunctionInfo& I, DeclTy* D)
{
    if(! extractInfo(I, D))
        return false;
    LineNumber = getLine(D);
    if(D->isThisDeclarationADefinition())
        I.DefLoc.emplace(LineNumber, File, IsFileInRootDir);
    else
        I.Loc.emplace_back(LineNumber, File, IsFileInRootDir);
    QualType const qt = D->getReturnType();
    std::string s = qt.getAsString();
    I.ReturnType = getTypeInfoForType(qt);
    parseParameters(I, D);

    getTemplateParams(I.Template, D);

    // Handle function template specializations.
    if(FunctionTemplateSpecializationInfo const* FTSI =
        D->getTemplateSpecializationInfo())
    {
        if(!I.Template)
            I.Template.emplace();
        I.Template->Specialization.emplace();
        auto& Specialization = *I.Template->Specialization;

        Specialization.SpecializationOf = getUSRForDecl(FTSI->getTemplate());

        // Template parameters to the specialization.
        if(FTSI->TemplateArguments)
        {
            for(TemplateArgument const& Arg :
                    FTSI->TemplateArguments->asArray())
            {
                Specialization.Params.emplace_back(*D, Arg);
            }
        }
    }

    //
    // FunctionDecl
    //
    I.specs0.isVariadic = D->isVariadic();
    I.specs0.isDefaulted = D->isDefaulted();
    I.specs0.isExplicitlyDefaulted = D->isExplicitlyDefaulted();
    I.specs0.isDeleted = D->isDeleted();
    I.specs0.isDeletedAsWritten = D->isDeletedAsWritten();
    I.specs0.isNoReturn = D->isNoReturn();
        // subsumes D->hasAttr<NoReturnAttr>()
        // subsumes D->hasAttr<CXX11NoReturnAttr>()
        // subsumes D->hasAttr<C11NoReturnAttr>()
        // subsumes D->getType()->getAs<FunctionType>()->getNoReturnAttr()
    I.specs0.hasOverrideAttr = D->template hasAttr<OverrideAttr>();
    if(auto const* FP = D->getType()->template getAs<FunctionProtoType>())
        I.specs0.hasTrailingReturn= FP->hasTrailingReturn();
    I.specs0.constexprKind = D->getConstexprKind();
        // subsumes D->isConstexpr();
        // subsumes D->isConstexprSpecified();
        // subsumes D->isConsteval();
    I.specs0.exceptionSpecType = D->getExceptionSpecType();
    I.specs0.overloadedOperator = D->getOverloadedOperator();
    I.specs0.storageClass = D->getStorageClass();
    if(auto attr = D->template getAttr<WarnUnusedResultAttr>())
    {
        I.specs1.isNodiscard = true;
        I.specs1.nodiscardSpelling = attr->getSemanticSpelling();
    }

    //
    // CXXMethodDecl
    //
    if constexpr(std::derived_from<DeclTy, CXXMethodDecl>)
    {
        I.specs0.isVirtual = D->isVirtual();
        I.specs0.isVirtualAsWritten = D->isVirtualAsWritten();
        I.specs0.isPure = D->isPure();
        I.specs0.isConst = D->isConst();
        I.specs0.isVolatile = D->isVolatile();
        I.specs0.refQualifier = D->getRefQualifier();
        I.specs0.isFinal = D->template hasAttr<FinalAttr>();
        //D->isCopyAssignmentOperator()
        //D->isMoveAssignmentOperator()
        //D->isOverloadedOperator();
        //D->isStaticOverloadedOperator();
    }

    //
    // CXXDestructorDecl
    //
    if constexpr(std::derived_from<DeclTy, CXXDestructorDecl>)
    {
        //I.Name.append("-dtor");
    }

    //
    // CXXConstructorDecl
    //
    if constexpr(std::derived_from<DeclTy, CXXConstructorDecl>)
    {
        //I.Name.append("-ctor");
        I.specs1.isExplicit = D->getExplicitSpecifier().isSpecified();
    }

    //
    // CXXConversionDecl
    //
    if constexpr(std::derived_from<DeclTy, CXXConversionDecl>)
    {
        //I.Name.append("-conv");
        I.specs1.isExplicit = D->getExplicitSpecifier().isSpecified();
    }

    //
    // CXXDeductionGuideDecl
    //
    if constexpr(std::derived_from<DeclTy, CXXDeductionGuideDecl>)
    {
        I.specs1.isExplicit = D->getExplicitSpecifier().isSpecified();
    }

    return true;
}

//------------------------------------------------

// Decl types which have isThisDeclarationADefinition:
//
// VarTemplateDecl
// FunctionTemplateDecl
// FunctionDecl
// TagDecl
// ClassTemplateDecl
// CXXDeductionGuideDecl

void
ASTVisitor::
buildNamespace(
    NamespaceDecl* D)
{
    if(! shouldExtract(D))
        return;
    NamespaceInfo I;
    if(! extractInfo(I, D))
        return;
    if(D->isAnonymousNamespace())
        I.Name = "@nonymous_namespace"; // VFALCO BAD!
    insertBitcode(ex_, writeBitcode(I));
    insertBitcode(ex_, writeParent(I));
}

void
ASTVisitor::
buildRecord(
    CXXRecordDecl* D)
{
    if(! shouldExtract(D))
        return;
    RecordInfo I;
    if(! extractInfo(I, D))
        return;
    LineNumber = getLine(D);
    if(D->isThisDeclarationADefinition())
        I.DefLoc.emplace(LineNumber, File, IsFileInRootDir);
    else
        I.Loc.emplace_back(LineNumber, File, IsFileInRootDir);
    I.TagType = D->getTagKind();
    parseFields(I, D, PublicOnly, AccessSpecifier::AS_public, R_);

    // These are from CXXRecordDecl::isEffectivelyFinal()
    I.specs.isFinal = D->template hasAttr<FinalAttr>();
    if(auto const DT = D->getDestructor())
        I.specs.isFinalDestructor = DT->template hasAttr<FinalAttr>();

    if(TypedefNameDecl const* TD = D->getTypedefNameForAnonDecl())
    {
        I.Name = TD->getNameAsString();
        I.IsTypeDef = true;
    }

    extractBases(I, D);

    getTemplateParams(I.Template, D);

    // Full and partial specializations.
    if(auto* CTSD = dyn_cast<ClassTemplateSpecializationDecl>(D))
    {
        if(!I.Template)
            I.Template.emplace();
        I.Template->Specialization.emplace();
        auto& Specialization = *I.Template->Specialization;

        // What this is a specialization of.
        auto SpecOf = CTSD->getSpecializedTemplateOrPartial();
        if(SpecOf.is<ClassTemplateDecl*>())
        {
            Specialization.SpecializationOf =
                getUSRForDecl(SpecOf.get<ClassTemplateDecl*>());
        }
        else if(SpecOf.is<ClassTemplatePartialSpecializationDecl*>())
        {
            Specialization.SpecializationOf =
                getUSRForDecl(SpecOf.get<ClassTemplatePartialSpecializationDecl*>());
        }

        // Parameters to the specilization. For partial specializations, get the
        // parameters "as written" from the ClassTemplatePartialSpecializationDecl
        // because the non-explicit template parameters will have generated internal
        // placeholder names rather than the names the user typed that match the
        // template parameters.
        if(ClassTemplatePartialSpecializationDecl const* CTPSD =
            dyn_cast<ClassTemplatePartialSpecializationDecl>(D))
        {
            if(ASTTemplateArgumentListInfo const* AsWritten =
                CTPSD->getTemplateArgsAsWritten())
            {
                for(unsigned i = 0; i < AsWritten->getNumTemplateArgs(); i++)
                {
                    Specialization.Params.emplace_back(
                        getSourceCode(D, (*AsWritten)[i].getSourceRange()));
                }
            }
        }
        else
        {
            for(TemplateArgument const& Arg : CTSD->getTemplateArgs().asArray())
            {
                Specialization.Params.emplace_back(*D, Arg);
            }
        }
    }

    AccessSpecifier access;
    if(auto CT = D->getDescribedClassTemplate())
    {
        access = CT->getAccess();
    }
    else if(auto MSI = D->getMemberSpecializationInfo())
    {
        access = MSI->getInstantiatedFrom()->getAccess();
    }
    else if(auto* CTSD = dyn_cast<ClassTemplateSpecializationDecl>(D))
    {
        access = CTSD->getSpecializedTemplate()->getAccess();
    }
    else
    {
        access = D->getAccess();
    }
    insertBitcode(ex_, writeBitcode(I));
    insertBitcode(ex_, writeParent(I, access));
}

void
ASTVisitor::
buildFriend(
    FriendDecl* D)
{
    if(NamedDecl* ND = D->getFriendDecl())
    {
        // D does not name a type
        if(FunctionDecl* FD = dyn_cast<FunctionDecl>(ND))
        {
            if(! shouldExtract(FD))
                return;
            FunctionInfo I;
            if(! constructFunction(I, FD))
                return;
            SymbolID id;
            getParent(id, D);
            RecordInfo P(id);
            P.Friends.emplace_back(I.id);
            bool isInAnonymous;
            getParentNamespaces(P.Namespace, ND, isInAnonymous);
            insertBitcode(ex_, writeBitcode(I));
            insertBitcode(ex_, writeParent(I));
            insertBitcode(ex_, writeBitcode(P));
            insertBitcode(ex_, writeParent(P));
            return;
        }
        if(FunctionTemplateDecl* FT = dyn_cast<FunctionTemplateDecl>(ND))
        {
            // VFALCO TODO
            (void)FT;
            return;
        }
        if(ClassTemplateDecl* CT = dyn_cast<ClassTemplateDecl>(ND))
        {
            // VFALCO TODO
            (void)CT;
            return;
        }

        Assert(false);
        return;
    }
    else if(TypeSourceInfo* TS = D->getFriendType())
    {
        (void)TS;
        return;
    }
    else
    {
        Assert(false);
    }
    return;
}

void
ASTVisitor::
buildEnum(
    EnumDecl* D)
{
    if(! shouldExtract(D))
        return;
    EnumInfo I;
    if(! extractInfo(I, D))
        return;
    LineNumber = getLine(D);
    if(D->isThisDeclarationADefinition())
        I.DefLoc.emplace(LineNumber, File, IsFileInRootDir);
    else
        I.Loc.emplace_back(LineNumber, File, IsFileInRootDir);
    I.Scoped = D->isScoped();
    if(D->isFixed())
    {
        auto Name = D->getIntegerType().getAsString();
        I.BaseType = TypeInfo(Name);
    }
    parseEnumerators(I, D);
    insertBitcode(ex_, writeBitcode(I));
    insertBitcode(ex_, writeParent(I, D->getAccess()));
}

void
ASTVisitor::
buildVar(
    VarDecl* D)
{
    if(! shouldExtract(D))
        return;
    VarInfo I;
    if(! extractInfo(I, D))
        return;
    LineNumber = getLine(D);
    if(D->isThisDeclarationADefinition())
        I.DefLoc.emplace(LineNumber, File, IsFileInRootDir);
    else
        I.Loc.emplace_back(LineNumber, File, IsFileInRootDir);
    static_cast<TypeInfo&>(I) =
        getTypeInfoForType(D->getTypeSourceInfo()->getType());
    I.specs.storageClass = D->getStorageClass();
    insertBitcode(ex_, writeBitcode(I));
    insertBitcode(ex_, writeParent(I, D->getAccess()));
}

template<class DeclTy>
requires std::derived_from<DeclTy, CXXMethodDecl>
void
ASTVisitor::
buildFunction(
    DeclTy* D)
{
    if(! shouldExtract(D))
        return;
    FunctionInfo I;
    if(! constructFunction(I, D))
        return;
    insertBitcode(ex_, writeBitcode(I));
    insertBitcode(ex_, writeParent(I, D->getAccess()));
}

template<class DeclTy>
requires (! std::derived_from<DeclTy, CXXMethodDecl>)
void
ASTVisitor::
buildFunction(
    DeclTy* D)
{
    if(! shouldExtract(D))
        return;
    FunctionInfo I;
    if(! constructFunction(I, D))
        return;
    insertBitcode(ex_, writeBitcode(I));
    insertBitcode(ex_, writeParent(I));
}

template<class DeclTy>
void
ASTVisitor::
buildTypedef(
    DeclTy* D)
{
    if(! shouldExtract(D))
        return;
    TypedefInfo I;
    if(! extractInfo(I, D))
        return;
    I.Underlying = getTypeInfoForType(D->getUnderlyingType());
    if(I.Underlying.Type.Name.empty())
    {
        // Typedef for an unnamed type. This is like
        // "typedef struct { } Foo;". The record serializer
        // explicitly checks for this syntax and constructs
        // a record with that name, so we don't want to emit
        // a duplicate here.
        return;
    }

    LineNumber = getLine(D);
    // D->isThisDeclarationADefinition(); // not available
    I.DefLoc.emplace(LineNumber, File, IsFileInRootDir);
    I.IsUsing = std::is_same_v<DeclTy, TypeAliasDecl>;
    insertBitcode(ex_, writeBitcode(I));
    insertBitcode(ex_, writeParent(I, D->getAccess()));
}

//------------------------------------------------

// An instance of Visitor runs on one translation unit.
void
ASTVisitor::
HandleTranslationUnit(
    ASTContext& Context)
{
    // This visitor is written expecting post-order
    Assert(shouldTraversePostOrder());

    // cache contextual variables
    astContext_ = &Context;
    sourceManager_ = &astContext_->getSourceManager();

    // Install handlers for our custom commands
    initCustomCommentCommands(Context);

    llvm::Optional<llvm::StringRef> filePath = 
        Context.getSourceManager().getNonBuiltinFilenameForID(
            Context.getSourceManager().getMainFileID());
    if(! filePath)
        return;

    // Filter out TUs we don't care about
    File = *filePath;
    convert_to_slash(File);
    if(! config_.shouldVisitTU(File))
        return;

    TraverseDecl(Context.getTranslationUnitDecl());
}

// Returning false from any of these
// functions will abort the _entire_ traversal

bool
ASTVisitor::
WalkUpFromNamespaceDecl(
    NamespaceDecl* D)
{
    buildNamespace(D);
    return true;
}

bool
ASTVisitor::
WalkUpFromCXXRecordDecl(
    CXXRecordDecl* D)
{
    buildRecord(D);
    return true;
}

bool
ASTVisitor::
WalkUpFromCXXMethodDecl(
    CXXMethodDecl* D)
{
    buildFunction(D);
    return true;
}

bool
ASTVisitor::
WalkUpFromCXXDestructorDecl(
    CXXDestructorDecl* D)
{
    buildFunction(D);
    return true;
}

bool
ASTVisitor::
WalkUpFromCXXConstructorDecl(
    CXXConstructorDecl* D)
{
    buildFunction(D);
    return true;
}

bool
ASTVisitor::
WalkUpFromCXXConversionDecl(
    CXXConversionDecl* D)
{
    buildFunction(D);
    return true;
}

bool
ASTVisitor::
WalkUpFromFunctionDecl(
    FunctionDecl* D)
{
    buildFunction(D);
    return true;
}

bool
ASTVisitor::
WalkUpFromFriendDecl(
    FriendDecl* D)
{
    buildFriend(D);
    return true;
}

bool
ASTVisitor::
WalkUpFromTypeAliasDecl(
    TypeAliasDecl* D)
{
    buildTypedef(D);
    return true;
}

bool
ASTVisitor::
WalkUpFromTypedefDecl(
    TypedefDecl* D)
{
    buildTypedef(D);
    return true;
}

bool
ASTVisitor::
WalkUpFromEnumDecl(
    EnumDecl* D)
{
    buildEnum(D);
    return true;
}

bool
ASTVisitor::
WalkUpFromVarDecl(
    VarDecl* D)
{
    buildVar(D);
    return true;
}

bool
ASTVisitor::
WalkUpFromParmVarDecl(
    ParmVarDecl* D)
{
    // apply the type adjustments specified in [dcl.fct] p5
    // to ensure that the USR of the corresponding function matches
    // other declarations of the function that have parameters declared
    // with different top-level cv-qualifiers
    // since we parse function parameters when we visit
    // the actual function declarations, do nothing else

    D->setType(astContext_->getSignatureParameterType(D->getType()));

    // Skip the VarDecl by not walking up from here
    return true;
}

} // mrdox
} // clang
