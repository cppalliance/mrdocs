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

#include "Serializer.hpp"
#include "Bitcode.hpp"
#include "ParseJavadoc.hpp"
#include <mrdox/Debug.hpp>
#include <mrdox/Metadata.hpp>
#include <clang/AST/DeclFriend.h>
#include <clang/Index/USRGeneration.h>
#include <clang/Lex/Lexer.h>
#include <llvm/ADT/Hashing.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/SHA1.h>
#include <cassert>

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>

namespace clang {
namespace mrdox {

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
    if(linkage == Linkage::ModuleLinkage ||
        linkage == Linkage::ExternalLinkage)
        return true;
    // some form of internal linkage
    return false; 
}

// handles TypedefDecl and TypeAliasDecl
static
bool
shouldSerializeInfo(
    bool PublicOnly,
    bool IsInAnonymousNamespace,
    TypedefNameDecl const* D)
{
    if(! PublicOnly)
        return true;
    if(IsInAnonymousNamespace)
        return false;
    if(auto const* N = dyn_cast<NamespaceDecl>(D))
        if(N->isAnonymousNamespace())
            return false;
    if(D->getAccessUnsafe() == AccessSpecifier::AS_private)
        return false;
    return true;
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
//
// Info
//
//------------------------------------------------

static
bool
getInfo(
    Serializer& sr,
    Info& I,
    NamedDecl const* D)
{
    bool IsInAnonymousNamespace;
    getParentNamespaces(
        I.Namespace, D, IsInAnonymousNamespace);
    if(! shouldSerializeInfo(
        sr.PublicOnly, IsInAnonymousNamespace, D))
        return false;
    I.id = getUSRForDecl(D);
    I.Name = D->getNameAsString();
    parseJavadoc(I.javadoc, D);
    return true;
}

static
bool
getInfo(
    Serializer& sr,
    Info& I,
    TypedefNameDecl const* D)
{
    bool IsInAnonymousNamespace;
    getParentNamespaces(
        I.Namespace, D, IsInAnonymousNamespace);
    if(! shouldSerializeInfo(
        sr.PublicOnly, IsInAnonymousNamespace, D))
        return false;
    I.id = getUSRForDecl(D);
    I.Name = D->getNameAsString();
    parseJavadoc(I.javadoc, D);
    return true;
}

//------------------------------------------------
//
// SymbolInfo
//
//------------------------------------------------

// The member function isThisDeclarationADefinition
// is non-virtual and only exists for certain concrete
// AST types, so we make this a function template.
template<class Decl>
static
bool
getSymbolInfo(
    Serializer& sr,
    SymbolInfo& I,
    Decl const* D)
{
    if(! getInfo(sr, I, D))
        return false;
    if(D->isThisDeclarationADefinition())
        I.DefLoc.emplace(sr.LineNumber, sr.File, sr.IsFileInRootDir);
    else
        I.Loc.emplace_back(sr.LineNumber, sr.File, sr.IsFileInRootDir);
    return true;
}

//------------------------------------------------
//
// FunctionInfo
//
//------------------------------------------------

static
bool
getFunctionInfo(
    Serializer& sr,
    FunctionInfo& I,
    FunctionDecl const* D)
{
    if(! getSymbolInfo(sr, I, D))
        return false;
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
    return true;
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
        parent.Children.Enums.emplace_back(std::move(I));
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

template<typename T>
static
void
getInfo(
    Info& I,
    T const* D,
    Javadoc jd,
    bool& IsInAnonymousNamespace,
    Reporter& R)
{
    I.id = getUSRForDecl(D);
    I.Name = D->getNameAsString();
    getParentNamespaces(
        I.Namespace,
        D,
        IsInAnonymousNamespace);
    I.javadoc = std::move(jd);
}

//------------------------------------------------

template <typename T>
static
void
getSymbolInfo(
    SymbolInfo& I,
    T const* D,
    Javadoc jd,
    int LineNumber,
    StringRef Filename,
    bool IsFileInRootDir,
    bool& IsInAnonymousNamespace,
    Reporter& R)
{
    getInfo(I, D, std::move(jd), IsInAnonymousNamespace, R);
    if(D->isThisDeclarationADefinition())
        I.DefLoc.emplace(LineNumber, Filename, IsFileInRootDir);
    else
        I.Loc.emplace_back(LineNumber, Filename, IsFileInRootDir);
}

//------------------------------------------------

static
void
getFunctionInfo(
    FunctionInfo& I,
    FunctionDecl const* D,
    Javadoc jd,
    int LineNumber,
    StringRef Filename,
    bool IsFileInRootDir,
    bool& IsInAnonymousNamespace,
    Reporter& R)
{
    getSymbolInfo(
        I, D, std::move(jd),
        LineNumber, Filename,
        IsFileInRootDir,
        IsInAnonymousNamespace,
        R);
    QualType const qt = D->getReturnType();
    std::string s = qt.getAsString();
    I.ReturnType = getTypeInfoForType(qt);
    parseParameters(I, D);

    getTemplateParams(I.Template, D);

    // Handle function template specializations.
    if(const FunctionTemplateSpecializationInfo* FTSI =
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
            for(const TemplateArgument& Arg :
                    FTSI->TemplateArguments->asArray())
            {
                Specialization.Params.emplace_back(*D, Arg);
            }
        }
    }
}

static
void
parseBases(
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
                        FunctionInfo FI;
                        FI.IsMethod = true;
                        // The seventh arg in getFunctionInfo is a boolean passed by
                        // reference, its value is not relevant in here so it's not used
                        // anywhere besides the function call.
                        bool IsInAnonymousNamespace;
                        getFunctionInfo(FI, MD, Javadoc(), /*LineNumber=*/{},
                            /*FileName=*/{}, IsFileInRootDir,
                            IsInAnonymousNamespace, R);
                        FI.Access =
                            getFinalAccessSpecifier(BI.Access, MD->getAccessUnsafe());
                        BI.Children.Functions.emplace_back(
                            FI.id,
                            FI.Name,
                            InfoType::IT_function);
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
                parseBases(I, Base, IsFileInRootDir, PublicOnly, false,
                    I.Bases.back().Access, R);
            #endif
            }
        }
    }
}

//------------------------------------------------

static
int
getLineNumber(
    NamedDecl const* D)
{
    return D->getASTContext()
        .getSourceManager()
        .getPresumedLoc(
            D->getBeginLoc()).getLine();
}

//------------------------------------------------

SerializeResult
Serializer::
build(
    NamespaceDecl const* D)
{
    NamespaceInfo I;
    if(! getInfo(*this, I, D))
        return {};

    if(D->isAnonymousNamespace())
        I.Name = "@nonymous_namespace";

    return {
        writeBitcode(I),
        writeParent(std::move(I)) };
}

static
void
getCXXRecordSpecs(
    RecordInfo& I,
    CXXRecordDecl const* D)
{
    // These are from CXXRecordDecl::isEffectivelyFinal()
    I.specs.set<RecFlags0::isFinal>(D->hasAttr<FinalAttr>());
    if(auto const DT = D->getDestructor())
    {
        I.specs.set<RecFlags0::isFinalDestructor>(DT->hasAttr<FinalAttr>());
    }
}

SerializeResult
Serializer::
build(
    CXXRecordDecl const* D)
{
    RecordInfo I;
    if(! getSymbolInfo(*this, I, D))
        return {};
    I.TagType = D->getTagKind();
    getCXXRecordSpecs(I, D);
    parseFields(I, D, PublicOnly, AccessSpecifier::AS_public, R_);
    if(auto const* C = dyn_cast<CXXRecordDecl>(D))
    {
        if(TypedefNameDecl const* TD = C->getTypedefNameForAnonDecl())
        {
            I.Name = TD->getNameAsString();
            I.IsTypeDef = true;
        }
        // VFALCO: remove first call to parseBases,
        // that function should be deleted
        parseBases(I, C); // VFALCO WHY?

        parseBases(
            I,
            C,
            IsFileInRootDir,
            PublicOnly,
            true,
            AccessSpecifier::AS_public,
            R_);
    }

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

    return { writeBitcode(I), writeParent(std::move(I)) };
}

//------------------------------------------------
//
// Functions, Member Functions
//
//------------------------------------------------

// VFALCO could this be done in getFunctionInfo?
//        but getFunctionInfo is called parseBases()
static
void
getFunctionSpecs(
    FunctionInfo& I,
    FunctionDecl const* D)
{
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
    I.specs0.set<FnFlags0::hasOverrideAttr>(D->hasAttr<OverrideAttr>());
    if(auto const* FP = D->getType()->getAs<FunctionProtoType>())
        I.specs0.set<FnFlags0::hasTrailingReturn>(FP->hasTrailingReturn());

    I.specs0.set<FnFlags0::constexprKind>(D->getConstexprKind());
        // subsumes D->isConstexpr();
        // subsumes D->isConstexprSpecified();
        // subsumes D->isConsteval();
    I.specs0.set<FnFlags0::exceptionSpecType>(D->getExceptionSpecType());
    I.specs0.set<FnFlags0::overloadedOperator>(D->getOverloadedOperator());
    I.specs0.set<FnFlags0::storageClass>(D->getStorageClass());

    if(auto const MF = dyn_cast<CXXMethodDecl>(D))
    {
        I.specs0.set<FnFlags0::isConst>(MF->isConst());
        I.specs0.set<FnFlags0::isVolatile>(MF->isVolatile());
        I.specs0.set<FnFlags0::refQualifier>(MF->getRefQualifier());
        //MF->isCopyAssignmentOperator()
        //MF->isMoveAssignmentOperator()
        //MF->isOverloadedOperator();
        //MF->isStaticOverloadedOperator();

        if(auto const Ctor = dyn_cast<CXXConstructorDecl>(MF))
        {
            I.specs1.set<FnFlags1::isExplicit>(Ctor->getExplicitSpecifier().isSpecified());
        }
        else if(auto const Conv = dyn_cast<CXXConversionDecl>(MF))
        {
            I.specs1.set<FnFlags1::isExplicit>(Conv->getExplicitSpecifier().isSpecified());
        }
    }
    else if(auto const DG = dyn_cast<CXXDeductionGuideDecl>(D))
    {
        I.specs1.set<FnFlags1::isExplicit>(DG->getExplicitSpecifier().isSpecified());
    }

    if(auto attr = D->getAttr<WarnUnusedResultAttr>())
    {
        I.specs1.set<FnFlags1::isNodiscard>(true);
        I.specs1.set<FnFlags1::nodiscardSpelling>(attr->getSemanticSpelling());
    }
}

static
bool
buildFunctionInfo(
    Serializer& sr,
    FunctionInfo& I,
    FunctionDecl const* D)
{
    if(! getFunctionInfo(sr, I, D))
        return false;
    I.Access = AccessSpecifier::AS_none;
    getFunctionSpecs(I, D);
    return true;
}

SerializeResult
Serializer::
build(
    CXXMethodDecl const* D)
{
    FunctionInfo I;
    if(! getFunctionInfo(*this, I, D))
        return {};
    getFunctionSpecs(I, D);

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

    getFunctionSpecs(I, D);

    return { writeBitcode(I), writeParent(std::move(I)) };
}

SerializeResult
Serializer::
build(
    FriendDecl const* D)
{
    if(NamedDecl const* ND = D->getFriendDecl())
    {
        // D does not name a type
        if(FunctionDecl const* FD = dyn_cast<FunctionDecl>(ND))
        {
            // VFALCO HACK, slam LineNumber before it is
            //        inserted into Loc or DefLoc
            LineNumber = getLineNumber(ND);

            FunctionInfo I;
            if(! buildFunctionInfo(*this, I, FD))
                return {};
            // VFALCO This is unfortunate, but the
            // default of 0 would be AS_public. see #84
            I.Access = AccessSpecifier::AS_none;
            SymbolID id;
            getParent(id, D);
            RecordInfo P(id);
            P.Friends.emplace_back(I.id);
            bool isInAnonymous;
            getParentNamespaces(P.Namespace, ND, isInAnonymous);
            return {
                writeBitcode(I),
                writeParent(std::move(I)),
                writeBitcode(P),
                writeParent(std::move(P)) };
        }
        if(FunctionTemplateDecl const* FT = dyn_cast<FunctionTemplateDecl>(ND))
        {
            // VFALCO TODO
            (void)FT;
            return {};
        }
        if(ClassTemplateDecl const* CT = dyn_cast<ClassTemplateDecl>(ND))
        {
            // VFALCO TODO
            (void)CT;
            return {};
        }

        Assert(false);
        return {};
    }
    else if(TypeSourceInfo const* TS = D->getFriendType())
    {
        (void)TS;
        return {};
    }
    else
    {
        Assert(false);
    }
    return {};
}

SerializeResult
Serializer::
build(
    UsingDecl const* D)
{
#if 0
    DeclarationNameInfo DNI = D->getNameInfo();
    NestedNameSpecifier const* NNS = D->getQualifier();
    //auto const& specs = NNS->getSpecifiers();
    bool hasTypename = D->hasTypename();
    bool isAccess = D->isAccessDeclaration();
    auto DN = DNI.getName();
    auto s = DNI.getAsString();
#endif
    return {};
}

SerializeResult
Serializer::
build(
    UsingShadowDecl const* D)
{
    return {};
}

SerializeResult
Serializer::
build(
    FunctionDecl const* D)
{
    FunctionInfo I;
    if(! buildFunctionInfo(*this, I, D))
        return {};
    return { writeBitcode(I), writeParent(std::move(I)) };
}

SerializeResult
Serializer::
build(
    TypedefDecl const* D)
{
    TypedefInfo I;
    if(! getInfo(*this, I, D))
        return {};
    I.Underlying = getTypeInfoForType(D->getUnderlyingType());
    if(I.Underlying.Type.Name.empty())
    {
        // Typedef for an unnamed type. This is like
        // "typedef struct { } Foo;". The record serializer
        // explicitly checks for this syntax and constructs
        // a record with that name, so we don't want to emit
        // a duplicate here.
        return {};
    }
    I.DefLoc.emplace(LineNumber, File, IsFileInRootDir);
    I.IsUsing = false;

    return { writeBitcode(I), writeParent(std::move(I)) };
}

// VFALCO This is basically a copy of the typedef
// builder except IsUsing=true, we need to refactor
// these.
//
SerializeResult
Serializer::
build(
    TypeAliasDecl const* D)
{
    TypedefInfo I;
    if(! getInfo(*this, I, D))
        return {};
    I.Underlying = getTypeInfoForType(D->getUnderlyingType());
    if(I.Underlying.Type.Name.empty())
    {
        // Typedef for an unnamed type. This is like
        // "typedef struct { } Foo;". The record serializer
        // explicitly checks for this syntax and constructs
        // a record with that name, so we don't want to emit
        // a duplicate here.
        return {};
    }
    I.DefLoc.emplace(LineNumber, File, IsFileInRootDir);
    I.IsUsing = true;

    return { writeBitcode(I), writeParent(std::move(I)) };
}

SerializeResult
Serializer::
build(
    EnumDecl const* D)
{
    EnumInfo I;
    if(! getSymbolInfo(*this, I, D))
        return {};
    I.Scoped = D->isScoped();
    if(D->isFixed())
    {
        auto Name = D->getIntegerType().getAsString();
        I.BaseType = TypeInfo(Name);
    }
    parseEnumerators(I, D);

    return { writeParent(std::move(I)) };
}

SerializeResult
Serializer::
build(
    VarDecl const* D)
{
    VariableInfo I;
    if(! getSymbolInfo(*this, I, D))
        return {};
    I.Type = getTypeInfoForType(D->getTypeSourceInfo()->getType());
#if 0
        MemberTypeInfo& NewMember = I.Members.emplace_back(
            getTypeInfoForType(F->getTypeSourceInfo()->getType()),
            F->getNameAsString(),
            getFinalAccessSpecifier(Access, F->getAccessUnsafe()));
        getMemberTypeInfo(NewMember, F, R);
#endif
    return {};
}

} // mrdox
} // clang
