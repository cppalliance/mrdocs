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
#include "ConfigImpl.hpp"
#include "ParseJavadoc.hpp"
#include "Support/Path.hpp"
#include <mrdox/Debug.hpp>
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
    , PublicOnly(! config_.includePrivate_)
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
        FieldTypeInfo& FieldInfo = I.Params.emplace_back(
            getTypeInfoForType(P->getOriginalType()),
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
    Decl const* D)
{
    // VFALCO investigate whether we can use
    // ASTContext::getCommentForDecl instead
    RawComment* RC =
        D->getASTContext().getRawCommentForDeclNoCache(D);
    if(RC)
    {
        RC->setAttached();
        javadoc.emplace(parseJavadoc(RC, D->getASTContext(), D));
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
    parseJavadoc(I.javadoc, D);
}

//------------------------------------------------

template<class Parent, class Child>
requires
    std::derived_from<Child, Info> &&
    std::is_same_v<decltype(Parent::Children), Scope>
static
void
insertChild(Parent& parent, Child&& I)
{
    if constexpr(std::is_same_v<Child, NamespaceInfo>)
    {
        // namespace requires parent namespace
        Assert(Parent::type_id == InfoType::IT_namespace);
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
        static_error("unknown Info type", I);
    }
}

// Create an empty parent for the child with the
// child inserted either as a reference or by moving
// the entire record. Then return the parent as a
// serialized bitcode.
template<class Child>
requires std::derived_from<Child, Info>
static
Bitcode
writeParent(Child&& I)
{
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
        insertChild(P, std::move(I));
        return writeBitcode(P);
    }
    if(I.Namespace[0].RefType == InfoType::IT_namespace)
    {
        NamespaceInfo P(I.Namespace[0].id);
        insertChild(P, std::move(I));
        return writeBitcode(P);
    }
    Assert(I.Namespace[0].RefType == InfoType::IT_record);
    Assert(Child::type_id != InfoType::IT_namespace);
    RecordInfo P(I.Namespace[0].id);
    insertChild(P, std::move(I));
    return writeBitcode(P);
}

// There are two uses for this function.
// 1) Getting the resulting mode of inheritance of a record.
//    Example: class A {}; class B : private A {}; class C : public B {};
//    It's explicit that C is publicly inherited from C and B is privately
//    inherited from A. It's not explicit but C is also privately inherited from
//    A. This is the AS that this function calculates. FirstAS is the
//    inheritance mode of `class C : B` and SecondAS is the inheritance mode of
//    `class B : A`.
// 2) Getting the inheritance mode of an inherited attribute / method.
//    Example : class A { public: int M; }; class B : private A {};
//    Class B is inherited from class A, which has a public attribute. This
//    attribute is now part of the derived class B but it's not public. This
//    will be private because the inheritance is private. This is the AS that
//    this function calculates. FirstAS is the inheritance mode and SecondAS is
//    the AS of the attribute / method.
static
AccessSpecifier
getFinalAccessSpecifier(
    AccessSpecifier FirstAS,
    AccessSpecifier SecondAS)
{
    if(FirstAS == AccessSpecifier::AS_none ||
        SecondAS == AccessSpecifier::AS_none)
        return AccessSpecifier::AS_none;
    if(FirstAS == AccessSpecifier::AS_private ||
        SecondAS == AccessSpecifier::AS_private)
        return AccessSpecifier::AS_private;
    if(FirstAS == AccessSpecifier::AS_protected ||
        SecondAS == AccessSpecifier::AS_protected)
        return AccessSpecifier::AS_protected;
    return AccessSpecifier::AS_public;
}

// The Access parameter is only provided when parsing the field of an inherited
// record, the access specification of the field depends on the inheritance mode
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
            getFinalAccessSpecifier(Access, F->getAccessUnsafe()));
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

// TODO: Remove the serialization of Parents and VirtualParents, this
// information is also extracted in the other definition of parseBases.
static
void
parseBases(
    ASTVisitor& sr,
    RecordInfo& I,
    CXXRecordDecl const* D)
{
    // Don't parse bases if this isn't a definition.
    if(!D->isThisDeclarationADefinition())
        return;
    for(const CXXBaseSpecifier& B : D->bases())
    {
        if(B.isVirtual())
            continue;
        if(auto const* Ty = B.getType()->getAs<TemplateSpecializationType>())
        {
            TemplateDecl const* D = Ty->getTemplateName().getAsTemplateDecl();
            I.Parents.emplace_back(
                getUSRForDecl(D),
                B.getType().getAsString(),
                InfoType::IT_record);
        }
        else if(RecordDecl const* P = getRecordDeclForType(B.getType()))
        {
            I.Parents.emplace_back(
                getUSRForDecl(P),
                P->getNameAsString(),
                InfoType::IT_record);
        }
        else
        {
            I.Parents.emplace_back(
                globalNamespaceID,
                B.getType().getAsString());
        }
    }
    for(CXXBaseSpecifier const& B : D->vbases())
    {
        if(RecordDecl const* P = getRecordDeclForType(B.getType()))
        {
            I.VirtualParents.emplace_back(
                getUSRForDecl(P),
                P->getNameAsString(),
                InfoType::IT_record);
        }
        else
        {
            I.VirtualParents.emplace_back(
                globalNamespaceID,
                B.getType().getAsString());
        }
    }
}

//------------------------------------------------

static
void
parseBases(
    ASTVisitor& sr,
    RecordInfo& I,
    CXXRecordDecl const* D,
    bool IsFileInRootDir,
    bool PublicOnly,
    bool IsParent,
    AccessSpecifier ParentAccess,
    Reporter& R)
{
    // Don't parse bases if this isn't a definition.
    if(!D->isThisDeclarationADefinition())
        return;
    for(const CXXBaseSpecifier& B : D->bases())
    {
        if(const RecordType* Ty = B.getType()->getAs<RecordType>())
        {
            if(const CXXRecordDecl* Base =
                cast_or_null<CXXRecordDecl>(Ty->getDecl()->getDefinition()))
            {
                // Initialized without USR and name, this will be set in the following
                // if-else stmt.
                BaseRecordInfo BI(
                    {}, "", B.isVirtual(),
                    getFinalAccessSpecifier(ParentAccess, B.getAccessSpecifier()),
                    IsParent);
                if(auto const* Ty = B.getType()->getAs<TemplateSpecializationType>())
                {
                    const TemplateDecl* D = Ty->getTemplateName().getAsTemplateDecl();
                    BI.id = getUSRForDecl(D);
                    BI.Name = B.getType().getAsString();
                }
                else
                {
                    BI.id = getUSRForDecl(Base);
                    BI.Name = Base->getNameAsString();
                }
                parseFields(BI, Base, PublicOnly, BI.Access, R);
                for(auto const& Decl : Base->decls())
                {
                    if(auto const* MD = dyn_cast<CXXMethodDecl>(Decl))
                    {
                        // Don't serialize private methods
                        if(MD->getAccessUnsafe() == AccessSpecifier::AS_private ||
                            !MD->isUserProvided())
                            continue;
                        BI.Children.Functions.emplace_back(sr.getFunctionReference(MD));
                    }
                }
                I.Bases.emplace_back(std::move(BI));
                // Call this function recursively to get the inherited classes of
                // this base; these new bases will also get stored in the original
                // RecordInfo: I.
                //
                // VFALCO Commented this out because we only want to show immediate
                //        bases. Alternatively, the generator could check IsParent
                //
            #if 0
                parseBases(sr, I, Base, IsFileInRootDir, PublicOnly, false,
                    I.Bases.back().Access, R);
            #endif
            }
        }
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
    parseJavadoc(I.javadoc, D);
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
    NamespaceDecl * D)
{
    if(! shouldExtract(D))
        return;
    NamespaceInfo I;
    if(! extractInfo(I, D))
        return;
    if(D->isAnonymousNamespace())
        I.Name = "@nonymous_namespace"; // VFALCO BAD!
    insertBitcode(ex_, writeBitcode(I));
    insertBitcode(ex_, writeParent(std::move(I)));
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
    I.specs.set<RecFlags0::isFinal>(D->template hasAttr<FinalAttr>());
    if(auto const DT = D->getDestructor())
        I.specs.set<RecFlags0::isFinalDestructor>(DT->template hasAttr<FinalAttr>());

    if(TypedefNameDecl const* TD = D->getTypedefNameForAnonDecl())
    {
        I.Name = TD->getNameAsString();
        I.IsTypeDef = true;
    }
    // VFALCO: remove first call to parseBases,
    // that function should be deleted
    parseBases(*this, I, D); // VFALCO WHY?

    parseBases(*this,
        I,
        D,
        IsFileInRootDir,
        PublicOnly,
        true,
        AccessSpecifier::AS_public,
        R_);

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

    insertBitcode(ex_, writeBitcode(I));
    insertBitcode(ex_, writeParent(std::move(I)));
}

template<class DeclTy>
bool
ASTVisitor::
buildFunction(
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
    I.specs0.set<FnFlags0::isVariadic>(D->isVariadic());
    I.specs0.set<FnFlags0::isVirtualAsWritten>(D->isVirtualAsWritten());
    I.specs0.set<FnFlags0::isPure>(D->isPure());
    I.specs0.set<FnFlags0::isDefaulted>(D->isDefaulted());
    I.specs0.set<FnFlags0::isExplicitlyDefaulted>(D->isExplicitlyDefaulted());
    I.specs0.set<FnFlags0::isDeleted>(D->isDeleted());
    I.specs0.set<FnFlags0::isDeletedAsWritten>(D->isDeletedAsWritten());
    I.specs0.set<FnFlags0::isNoReturn>(D->isNoReturn());
        // subsumes D->hasAttr<NoReturnAttr>()
        // subsumes D->hasAttr<CXX11NoReturnAttr>()
        // subsumes D->hasAttr<C11NoReturnAttr>()
        // subsumes D->getType()->getAs<FunctionType>()->getNoReturnAttr()
    I.specs0.set<FnFlags0::hasOverrideAttr>(D->template hasAttr<OverrideAttr>());
    if(auto const* FP = D->getType()->template getAs<FunctionProtoType>())
        I.specs0.set<FnFlags0::hasTrailingReturn>(FP->hasTrailingReturn());
    I.specs0.set<FnFlags0::constexprKind>(D->getConstexprKind());
        // subsumes D->isConstexpr();
        // subsumes D->isConstexprSpecified();
        // subsumes D->isConsteval();
    I.specs0.set<FnFlags0::exceptionSpecType>(D->getExceptionSpecType());
    I.specs0.set<FnFlags0::overloadedOperator>(D->getOverloadedOperator());
    I.specs0.set<FnFlags0::storageClass>(D->getStorageClass());
    if(auto attr = D->template getAttr<WarnUnusedResultAttr>())
    {
        I.specs1.set<FnFlags1::isNodiscard>(true);
        I.specs1.set<FnFlags1::nodiscardSpelling>(attr->getSemanticSpelling());
    }

    if constexpr(! std::derived_from<DeclTy, CXXMethodDecl>)
    {
        I.IsMethod = false;
        I.Access = AccessSpecifier::AS_none;
    }

    //
    // CXXMethodDecl
    //
    if constexpr(std::derived_from<DeclTy, CXXMethodDecl>)
    {
        I.IsMethod = true;
        NamedDecl const* PD = nullptr;
        if(auto const* SD = dyn_cast<ClassTemplateSpecializationDecl>(D->getParent()))
            PD = SD->getSpecializedTemplate();
        else
            PD = D->getParent();
        SymbolID ParentID = getUSRForDecl(PD);
        I.Parent = Reference(
            ParentID,
            PD->getNameAsString(),
            InfoType::IT_record);
        I.Access = D->getAccess();

        I.specs0.set<FnFlags0::isConst>(D->isConst());
        I.specs0.set<FnFlags0::isVolatile>(D->isVolatile());
        I.specs0.set<FnFlags0::refQualifier>(D->getRefQualifier());
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
        I.specs1.set<FnFlags1::isExplicit>(D->getExplicitSpecifier().isSpecified());
    }

    //
    // CXXConversionDecl
    //
    if constexpr(std::derived_from<DeclTy, CXXConversionDecl>)
    {
        //I.Name.append("-conv");
        I.specs1.set<FnFlags1::isExplicit>(D->getExplicitSpecifier().isSpecified());
    }

    //
    // CXXDeductionGuideDecl
    //
    if constexpr(std::derived_from<DeclTy, CXXDeductionGuideDecl>)
    {
        I.specs1.set<FnFlags1::isExplicit>(D->getExplicitSpecifier().isSpecified());
    }

    return true;
}

template<class DeclTy>
void
ASTVisitor::
buildFunction(
    DeclTy* D)
{
    if(! shouldExtract(D))
        return;
    FunctionInfo I;
    if(! buildFunction(I, D))
        return;
    insertBitcode(ex_, writeBitcode(I));
    insertBitcode(ex_, writeParent(std::move(I)));
}

template<class DeclTy>
void
ASTVisitor::
buildFriend(
    DeclTy* D)
{
    if(NamedDecl* ND = D->getFriendDecl())
    {
        // D does not name a type
        if(FunctionDecl* FD = dyn_cast<FunctionDecl>(ND))
        {
            if(! shouldExtract(FD))
                return;
            FunctionInfo I;
            if(! buildFunction(I, FD))
                return;
            // VFALCO This is unfortunate, but the
            // default of 0 would be AS_public. see #84
            I.Access = AccessSpecifier::AS_none;
            SymbolID id;
            getParent(id, D);
            RecordInfo P(id);
            P.Friends.emplace_back(I.id);
            bool isInAnonymous;
            getParentNamespaces(P.Namespace, ND, isInAnonymous);
            insertBitcode(ex_, writeBitcode(I));
            insertBitcode(ex_, writeParent(std::move(I)));
            insertBitcode(ex_, writeBitcode(P));
            insertBitcode(ex_, writeParent(std::move(P)));
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
    insertBitcode(ex_, writeParent(std::move(I)));
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
    insertBitcode(ex_, writeParent(std::move(I)));
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
    I.specs.set<VarFlags0::storageClass>(D->getStorageClass());
    insertBitcode(ex_, writeBitcode(I));
    insertBitcode(ex_, writeParent(std::move(I)));
}

//------------------------------------------------

// An instance of Visitor runs on one translation unit.
void
ASTVisitor::
HandleTranslationUnit(
    ASTContext& Context)
{
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
    // We do nothing here, to prevent ParmVarDecl
    // from appearing as VarDecl. We pick up the
    // function parameters as a group from the
    // FunctionDecl instead of visiting ParmVarDecl.
    return true;
}

} // mrdox
} // clang
