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

#include "Serialize.hpp"
#include "BitcodeWriter.hpp"
#include "ParseJavadoc.hpp"
#include <mrdox/Metadata.hpp>
#include <clang/Index/USRGeneration.h>
#include <clang/Lex/Lexer.h>
#include <llvm/ADT/Hashing.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/SHA1.h>

namespace clang {
namespace mrdox {

// Function to hash a given USR value for storage.
// As USRs (Unified Symbol Resolution) could be large, especially for functions
// with long type arguments, we use 160-bits SHA1(USR) values to
// guarantee the uniqueness of symbols while using a relatively small amount of
// memory (vs storing USRs directly).
SymbolID
hashUSR(
    llvm::StringRef USR)
{
    return llvm::SHA1::hash(arrayRefFromStringRef(USR));
}

template <typename T>
static
void
populateParentNamespaces(
    llvm::SmallVector<Reference, 4>& Namespaces,
    const T* D,
    bool& IsAnonymousNamespace);

static
void
populateMemberTypeInfo(
    MemberTypeInfo& I,
    const FieldDecl* D,
    Reporter& R);

// A function to extract the appropriate relative path for a given info's
// documentation. The path returned is a composite of the parent namespaces.
//
// Example: Given the below, the directory path for class C info will be
// <root>/A/B
//
// namespace A {
// namespace B {
//
// class C {};
//
// }
// }
llvm::SmallString<128>
getInfoRelativePath(
    llvm::SmallVectorImpl<mrdox::Reference> const& Namespaces)
{
    llvm::SmallString<128> Path;
    for (auto R = Namespaces.rbegin(), E = Namespaces.rend(); R != E; ++R)
        llvm::sys::path::append(Path, R->Name);
    return Path;
}

llvm::SmallString<128>
getInfoRelativePath(const Decl* D)
{
    llvm::SmallVector<Reference, 4> Namespaces;
    // The third arg in populateParentNamespaces is a boolean passed by reference,
    // its value is not relevant in here so it's not used anywhere besides the
    // function call
    bool B = true;
    populateParentNamespaces(Namespaces, D, B);
    return getInfoRelativePath(Namespaces);
}

// Serializing functions.

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

template<typename T>
static
std::string
serialize(T& I)
{
    SmallString<2048> Buffer;
    llvm::BitstreamWriter Stream(Buffer);
    BitcodeWriter Writer(Stream);
    Writer.emitBlock(I);
    return Buffer.str().str();
}

std::string
serialize(Info const& I)
{
    switch (I.IT)
    {
    case InfoType::IT_namespace:
        return serialize(*static_cast<NamespaceInfo const*>(&I));
    case InfoType::IT_record:
        return serialize(*static_cast<RecordInfo const*>(&I));
    case InfoType::IT_enum:
        return serialize(*static_cast<EnumInfo const*>(&I));
    case InfoType::IT_function:
        return serialize(*static_cast<FunctionInfo const*>(&I));
    //case InfoType::IT_typedef:
        //return serialize(*static_cast<TypedefInfo const*>(&I));
    default:
        return "";
    }
}

static
SymbolID
getUSRForDecl(
    Decl const* D){
    llvm::SmallString<128> USR;
    if (index::generateUSRForDecl(D, USR))
        return SymbolID();
    return hashUSR(USR);
}

static TagDecl* getTagDeclForType(const QualType& T) {
    if (const TagDecl* D = T->getAsTagDecl())
        return D->getDefinition();
    return nullptr;
}

static RecordDecl* getRecordDeclForType(const QualType& T) {
    if (const RecordDecl* D = T->getAsRecordDecl())
        return D->getDefinition();
    return nullptr;
}

TypeInfo
getTypeInfoForType(
    QualType const& T)
{
    TagDecl const* TD = getTagDeclForType(T);
    if (!TD)
        return TypeInfo(Reference(
            SymbolID(), T.getAsString()));
    InfoType IT;
    if (dyn_cast<EnumDecl>(TD))
        IT = InfoType::IT_enum;
    else if (dyn_cast<RecordDecl>(TD))
        IT = InfoType::IT_record;
    else
        IT = InfoType::IT_default;
    return TypeInfo(Reference(
        getUSRForDecl(TD),
        TD->getNameAsString(),
        IT,
        getInfoRelativePath(TD)));
}

static
bool
isPublic(
    clang::AccessSpecifier const AS,
    clang::Linkage const Link)
{
    if (AS == clang::AccessSpecifier::AS_private)
        return false;
    if (Link == clang::Linkage::ModuleLinkage ||
        Link == clang::Linkage::ExternalLinkage)
        return true;
    // some form of internal linkage
    return false; 
}

static
bool
shouldSerializeInfo(
    bool PublicOnly,
    bool IsInAnonymousNamespace,
    NamedDecl const* D)
{
    bool IsAnonymousNamespace = false;
    if(auto const* N = dyn_cast<NamespaceDecl>(D))
        IsAnonymousNamespace = N->isAnonymousNamespace();
    return !PublicOnly ||
        (!IsInAnonymousNamespace && !IsAnonymousNamespace &&
            isPublic(D->getAccessUnsafe(), D->getLinkageInternal()));
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
    {
        if(N->isAnonymousNamespace())
            return false;
    }
    if(D->getAccessUnsafe() == AccessSpecifier::AS_private)
        return false;
    return true;
}

// The InsertChild functions insert the given info into the given scope using
// the method appropriate for that type. Some types are moved into the
// appropriate vector, while other types have Reference objects generated to
// refer to them.
//
// See MakeAndInsertIntoParent().
static
void
InsertChild(
    Scope& scope,
    NamespaceInfo const& Info)
{
    scope.Namespaces.emplace_back(
        Info.USR,
        Info.Name,
        InfoType::IT_namespace,
        getInfoRelativePath(Info.Namespace));
}

static
void
InsertChild(
    Scope& scope,
    RecordInfo const& Info)
{
    scope.Records.emplace_back(
        Info.USR,
        Info.Name,
        InfoType::IT_record,
        getInfoRelativePath(Info.Namespace));
}

static
void
InsertChild(
    Scope& scope,
    FunctionInfo const& Info)
{
    scope.Functions.emplace_back(
        Info.USR,
        Info.Name,
        InfoType::IT_function,
        getInfoRelativePath(Info.Namespace));
}

static
void
InsertChild(
    Scope& scope,
    EnumInfo Info)
{
    scope.Enums.push_back(std::move(Info));
}

static
void
InsertChild(
    Scope& scope,
    TypedefInfo Info)
{
    scope.Typedefs.push_back(std::move(Info));
}

// Creates a parent of the correct type for the given child and inserts it into
// that parent.
//
// This is complicated by the fact that namespaces and records are inserted by
// reference (constructing a "Reference" object with that namespace/record's
// info), while everything else is inserted by moving it directly into the child
// vectors.
//
// For namespaces and records, explicitly specify a const& template parameter
// when invoking this function:
//   MakeAndInsertIntoParent<const Record&>(...);
// Otherwise, specify an rvalue reference <EnumInfo&&> and move into the
// parameter. Since each variant is used once, it's not worth having a more
// elaborate system to automatically deduce this information.
template<class ChildType>
std::unique_ptr<Info>
MakeAndInsertIntoParent(
    ChildType Child)
{
    if (Child.Namespace.empty())
    {
        // Insert into unnamed parent namespace.
        auto ParentNS = std::make_unique<NamespaceInfo>();
        InsertChild(ParentNS->Children, std::forward<ChildType>(Child));
        return ParentNS;
    }

    switch (Child.Namespace[0].RefType)
    {
    case InfoType::IT_namespace: {
        auto ParentNS = std::make_unique<NamespaceInfo>();
        ParentNS->USR = Child.Namespace[0].USR;
        InsertChild(ParentNS->Children, std::forward<ChildType>(Child));
        return ParentNS;
    }
    case InfoType::IT_record: {
        auto ParentRec = std::make_unique<RecordInfo>();
        ParentRec->USR = Child.Namespace[0].USR;
        InsertChild(ParentRec->Children, std::forward<ChildType>(Child));
        return ParentRec;
    }
    default:
        llvm_unreachable("Invalid reference type for parent namespace");
    }
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
    if (FirstAS == AccessSpecifier::AS_none ||
        SecondAS == AccessSpecifier::AS_none)
        return AccessSpecifier::AS_none;
    if (FirstAS == AccessSpecifier::AS_private ||
        SecondAS == AccessSpecifier::AS_private)
        return AccessSpecifier::AS_private;
    if (FirstAS == AccessSpecifier::AS_protected ||
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
    for (const FieldDecl* F : D->fields())
    {
        if (!shouldSerializeInfo(PublicOnly, /*IsInAnonymousNamespace=*/false, F))
            continue;

        // Use getAccessUnsafe so that we just get the default AS_none if it's not
        // valid, as opposed to an assert.
        MemberTypeInfo& NewMember = I.Members.emplace_back(
            getTypeInfoForType(F->getTypeSourceInfo()->getType()),
            F->getNameAsString(),
            getFinalAccessSpecifier(Access, F->getAccessUnsafe()));
        populateMemberTypeInfo(NewMember, F, R);
    }
}

static
void
parseEnumerators(
    EnumInfo& I,
    const EnumDecl* D)
{
    for (const EnumConstantDecl* E : D->enumerators())
    {
        std::string ValueExpr;
        if (const Expr* InitExpr = E->getInitExpr())
            ValueExpr = getSourceCode(D, InitExpr->getSourceRange());

        SmallString<16> ValueStr;
        E->getInitVal().toString(ValueStr);
        I.Members.emplace_back(E->getNameAsString(), ValueStr, ValueExpr);
    }
}

static void parseParameters(FunctionInfo& I, const FunctionDecl* D) {
    for (const ParmVarDecl* P : D->parameters()) {
        FieldTypeInfo& FieldInfo = I.Params.emplace_back(
            getTypeInfoForType(P->getOriginalType()), P->getNameAsString());
        FieldInfo.DefaultValue = getSourceCode(D, P->getDefaultArgRange());
    }
}

// TODO: Remove the serialization of Parents and VirtualParents, this
// information is also extracted in the other definition of parseBases.
static void parseBases(RecordInfo& I, const CXXRecordDecl* D) {
    // Don't parse bases if this isn't a definition.
    if (!D->isThisDeclarationADefinition())
        return;
    for (const CXXBaseSpecifier& B : D->bases()) {
        if (B.isVirtual())
            continue;
        if (const auto* Ty = B.getType()->getAs<TemplateSpecializationType>()) {
            const TemplateDecl* D = Ty->getTemplateName().getAsTemplateDecl();
            I.Parents.emplace_back(getUSRForDecl(D), B.getType().getAsString(),
                InfoType::IT_record, B.getType().getAsString());
        }
        else if (const RecordDecl* P = getRecordDeclForType(B.getType()))
            I.Parents.emplace_back(getUSRForDecl(P), P->getNameAsString(),
                InfoType::IT_record, getInfoRelativePath(P));
        else
            I.Parents.emplace_back(SymbolID(), B.getType().getAsString());
    }
    for (const CXXBaseSpecifier& B : D->vbases()) {
        if (const RecordDecl* P = getRecordDeclForType(B.getType()))
            I.VirtualParents.emplace_back(
                getUSRForDecl(P), P->getNameAsString(), InfoType::IT_record,
                    getInfoRelativePath(P));
        else
            I.VirtualParents.emplace_back(SymbolID(), B.getType().getAsString());
    }
}

template <typename T>
static
void
populateParentNamespaces(
    llvm::SmallVector<Reference, 4>& Namespaces,
    const T* D,
    bool& IsInAnonymousNamespace)
{
    const DeclContext* DC = D->getDeclContext();
    do {
        if (const auto* N = dyn_cast<NamespaceDecl>(DC)) {
            std::string Namespace;
            if (N->isAnonymousNamespace()) {
                Namespace = "@nonymous_namespace";
                IsInAnonymousNamespace = true;
            }
            else
                Namespace = N->getNameAsString();
            Namespaces.emplace_back(getUSRForDecl(N), Namespace,
                InfoType::IT_namespace,
                N->getQualifiedNameAsString());
        }
        else if (const auto* N = dyn_cast<RecordDecl>(DC))
            Namespaces.emplace_back(getUSRForDecl(N), N->getNameAsString(),
                InfoType::IT_record,
                N->getQualifiedNameAsString());
        else if (const auto* N = dyn_cast<FunctionDecl>(DC))
            Namespaces.emplace_back(getUSRForDecl(N), N->getNameAsString(),
                InfoType::IT_function,
                N->getQualifiedNameAsString());
        else if (const auto* N = dyn_cast<EnumDecl>(DC))
            Namespaces.emplace_back(getUSRForDecl(N), N->getNameAsString(),
                InfoType::IT_enum, N->getQualifiedNameAsString());
    } while ((DC = DC->getParent()));
    // The global namespace should be added to the list of namespaces if the decl
    // corresponds to a Record and if it doesn't have any namespace (because this
    // means it's in the global namespace). Also if its outermost namespace is a
    // record because that record matches the previous condition mentioned.
    if ((Namespaces.empty() && isa<RecordDecl>(D)) ||
        (!Namespaces.empty() && Namespaces.back().RefType == InfoType::IT_record))
        Namespaces.emplace_back(
            SymbolID(),
            "", //"GlobalNamespace",
            InfoType::IT_namespace);
}

void PopulateTemplateParameters(llvm::Optional<TemplateInfo>& TemplateInfo,
    const clang::Decl* D) {
    if (const TemplateParameterList* ParamList =
        D->getDescribedTemplateParams()) {
        if (!TemplateInfo) {
            TemplateInfo.emplace();
        }
        for (const NamedDecl* ND : *ParamList)
        {
            TemplateInfo->Params.emplace_back(*ND);
        }
    }
}

//------------------------------------------------

template<typename T>
static
void
populateInfo(
    Info& I,
    T const* D,
    Javadoc jd,
    bool& IsInAnonymousNamespace,
    Reporter& R)
{
    I.USR = getUSRForDecl(D);
    I.Name = D->getNameAsString();
    populateParentNamespaces(
        I.Namespace,
        D,
        IsInAnonymousNamespace);
    I.javadoc = std::move(jd);
}

//------------------------------------------------

template <typename T>
static
void
populateSymbolInfo(
    SymbolInfo& I,
    T const* D,
    Javadoc jd,
    int LineNumber,
    StringRef Filename,
    bool IsFileInRootDir,
    bool& IsInAnonymousNamespace,
    Reporter& R)
{
    populateInfo(I, D, std::move(jd), IsInAnonymousNamespace, R);
    if (D->isThisDeclarationADefinition())
        I.DefLoc.emplace(LineNumber, Filename, IsFileInRootDir);
    else
        I.Loc.emplace_back(LineNumber, Filename, IsFileInRootDir);
}

//------------------------------------------------

static
void
populateFunctionInfo(
    FunctionInfo& I,
    FunctionDecl const* D,
    Javadoc jd,
    int LineNumber,
    StringRef Filename,
    bool IsFileInRootDir,
    bool& IsInAnonymousNamespace,
    Reporter& R)
{
    populateSymbolInfo(
        I, D, std::move(jd),
        LineNumber, Filename,
        IsFileInRootDir,
        IsInAnonymousNamespace,
        R);
    QualType const qt = D->getReturnType();
    std::string s = qt.getAsString();
    I.ReturnType = getTypeInfoForType(qt);
    parseParameters(I, D);

    PopulateTemplateParameters(I.Template, D);

    // Handle function template specializations.
    if (const FunctionTemplateSpecializationInfo* FTSI =
        D->getTemplateSpecializationInfo())
    {
        if (!I.Template)
            I.Template.emplace();
        I.Template->Specialization.emplace();
        auto& Specialization = *I.Template->Specialization;

        Specialization.SpecializationOf = getUSRForDecl(FTSI->getTemplate());

        // Template parameters to the specialization.
        if (FTSI->TemplateArguments)
        {
            for (const TemplateArgument& Arg :
                    FTSI->TemplateArguments->asArray())
            {
                Specialization.Params.emplace_back(*D, Arg);
            }
        }
    }
}

static
void
populateMemberTypeInfo(
    MemberTypeInfo& I,
    const FieldDecl* D,
    Reporter& R)
{
    assert(D && "Expect non-null FieldDecl in populateMemberTypeInfo");

    ASTContext& Context = D->getASTContext();
    // TODO investigate whether we can use ASTContext::getCommentForDecl instead
    // of this logic. See also similar code in Mapper.cpp.
    RawComment* RC = Context.getRawCommentForDeclNoCache(D);
    if(RC)
    {
        RC->setAttached();
        I.javadoc = parseJavadoc(RC, D->getASTContext(), D);
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
    if (!D->isThisDeclarationADefinition())
        return;
    for (const CXXBaseSpecifier& B : D->bases())
    {
        if (const RecordType* Ty = B.getType()->getAs<RecordType>())
        {
            if (const CXXRecordDecl* Base =
                cast_or_null<CXXRecordDecl>(Ty->getDecl()->getDefinition()))
            {
                // Initialized without USR and name, this will be set in the following
                // if-else stmt.
                BaseRecordInfo BI(
                    {}, "", getInfoRelativePath(Base), B.isVirtual(),
                    getFinalAccessSpecifier(ParentAccess, B.getAccessSpecifier()),
                    IsParent);
                if (const auto* Ty = B.getType()->getAs<TemplateSpecializationType>())
                {
                    const TemplateDecl* D = Ty->getTemplateName().getAsTemplateDecl();
                    BI.USR = getUSRForDecl(D);
                    BI.Name = B.getType().getAsString();
                }
                else
                {
                    BI.USR = getUSRForDecl(Base);
                    BI.Name = Base->getNameAsString();
                }
                parseFields(BI, Base, PublicOnly, BI.Access, R);
                for (const auto& Decl : Base->decls())
                {
                    if (const auto* MD = dyn_cast<CXXMethodDecl>(Decl))
                    {
                        // Don't serialize private methods
                        if (MD->getAccessUnsafe() == AccessSpecifier::AS_private ||
                            !MD->isUserProvided())
                            continue;
                        FunctionInfo FI;
                        FI.IsMethod = true;
                        // The seventh arg in populateFunctionInfo is a boolean passed by
                        // reference, its value is not relevant in here so it's not used
                        // anywhere besides the function call.
                        bool IsInAnonymousNamespace;
                        populateFunctionInfo(FI, MD, Javadoc(), /*LineNumber=*/{},
                            /*FileName=*/{}, IsFileInRootDir,
                            IsInAnonymousNamespace, R);
                        FI.Access =
                            getFinalAccessSpecifier(BI.Access, MD->getAccessUnsafe());
                        BI.Children.Functions.emplace_back(
                            FI.USR,
                            FI.Name,
                            InfoType::IT_function,
                            FI.Path);
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

auto
Serializer::
buildInfo(
    NamespaceDecl const* D) ->
        value_type
{
    auto I = std::make_unique<NamespaceInfo>();
    bool IsInAnonymousNamespace = false;
    populateInfo(*I, D, std::move(jd_), IsInAnonymousNamespace, R_);
    if (!shouldSerializeInfo(PublicOnly, IsInAnonymousNamespace, D))
        return {};

    if(D->isAnonymousNamespace())
        I->Name = "@nonymous_namespace";
    I->Path = getInfoRelativePath(I->Namespace);
    if (I->Namespace.empty() && I->USR == SymbolID())
        return { std::unique_ptr<Info>{std::move(I)}, nullptr };

    // Namespaces are inserted into the parent by reference, so we need to return
    // both the parent and the record itself.
    return { std::move(I), MakeAndInsertIntoParent<const NamespaceInfo&>(*I) };
}

auto
Serializer::
buildInfo(
    CXXRecordDecl const* D) ->
        value_type
{
    auto I = std::make_unique<RecordInfo>();
    bool IsInAnonymousNamespace = false;
    populateSymbolInfo(*I, D, std::move(jd_), LineNumber, File, IsFileInRootDir,
        IsInAnonymousNamespace, R_);
    if (!shouldSerializeInfo(PublicOnly, IsInAnonymousNamespace, D))
        return {};

    I->TagType = D->getTagKind();
    parseFields(*I, D, PublicOnly, AccessSpecifier::AS_public, R_);
    if (const auto* C = dyn_cast<CXXRecordDecl>(D))
    {
        if (const TypedefNameDecl* TD = C->getTypedefNameForAnonDecl())
        {
            I->Name = TD->getNameAsString();
            I->IsTypeDef = true;
        }
        // TODO: remove first call to parseBases, that function should be deleted
        parseBases(*I, C);
        parseBases(*I, C, IsFileInRootDir, PublicOnly, true, AccessSpecifier::AS_public, R_);
    }
    I->Path = getInfoRelativePath(I->Namespace);

    PopulateTemplateParameters(I->Template, D);

    // Full and partial specializations.
    if (auto* CTSD = dyn_cast<ClassTemplateSpecializationDecl>(D))
    {
        if (!I->Template)
            I->Template.emplace();
        I->Template->Specialization.emplace();
        auto& Specialization = *I->Template->Specialization;

        // What this is a specialization of.
        auto SpecOf = CTSD->getSpecializedTemplateOrPartial();
        if (SpecOf.is<ClassTemplateDecl*>())
        {
            Specialization.SpecializationOf =
                getUSRForDecl(SpecOf.get<ClassTemplateDecl*>());
        }
        else if (SpecOf.is<ClassTemplatePartialSpecializationDecl*>())
        {
            Specialization.SpecializationOf =
                getUSRForDecl(SpecOf.get<ClassTemplatePartialSpecializationDecl*>());
        }

        // Parameters to the specilization. For partial specializations, get the
        // parameters "as written" from the ClassTemplatePartialSpecializationDecl
        // because the non-explicit template parameters will have generated internal
        // placeholder names rather than the names the user typed that match the
        // template parameters.
        if (const ClassTemplatePartialSpecializationDecl* CTPSD =
            dyn_cast<ClassTemplatePartialSpecializationDecl>(D))
        {
            if (const ASTTemplateArgumentListInfo* AsWritten =
                CTPSD->getTemplateArgsAsWritten())
            {
                for (unsigned i = 0; i < AsWritten->getNumTemplateArgs(); i++) {
                    Specialization.Params.emplace_back(
                        getSourceCode(D, (*AsWritten)[i].getSourceRange()));
                }
            }
        }
        else
        {
            for (const TemplateArgument& Arg : CTSD->getTemplateArgs().asArray())
            {
                Specialization.Params.emplace_back(*D, Arg);
            }
        }
    }

    // Functions are inserted into the parent by
    // reference, so we need to return both the
    // parent and the record itself.
    return { std::move(I), MakeAndInsertIntoParent<const RecordInfo&>(*I) };
}

auto
Serializer::
buildInfo(
    FunctionDecl const* D) ->
        value_type
{
    auto up = std::make_unique<FunctionInfo>();
    bool IsInAnonymousNamespace = false;
    populateFunctionInfo(*up, D, std::move(jd_), LineNumber, File, IsFileInRootDir,
        IsInAnonymousNamespace, R_);
    up->Access = clang::AccessSpecifier::AS_none;
    if (!shouldSerializeInfo(PublicOnly, IsInAnonymousNamespace, D))
        return {};

    // Functions are inserted into the parent by
    // reference, so we need to return both the
    // parent and the record itself.
    return { std::move(up), MakeAndInsertIntoParent<FunctionInfo const&>(*up) };
}

auto
Serializer::
buildInfo(
    CXXMethodDecl const* D) ->
        value_type
{
    auto up = std::make_unique<FunctionInfo>();
    bool IsInAnonymousNamespace = false;
    populateFunctionInfo(*up, D, std::move(jd_), LineNumber, File, IsFileInRootDir,
        IsInAnonymousNamespace, R_);
    if (!shouldSerializeInfo(PublicOnly, IsInAnonymousNamespace, D))
        return {};

    up->IsMethod = true;

    const NamedDecl* Parent = nullptr;
    if (const auto* SD = dyn_cast<ClassTemplateSpecializationDecl>(D->getParent()))
        Parent = SD->getSpecializedTemplate();
    else
        Parent = D->getParent();

    SymbolID ParentUSR = getUSRForDecl(Parent);
    up->Parent = Reference{
        ParentUSR, Parent->getNameAsString(), InfoType::IT_record,
            Parent->getQualifiedNameAsString() };
    up->Access = D->getAccess();

    // Functions are inserted into the parent by
    // reference, so we need to return both the
    // parent and the record itself.
    return { std::move(up), MakeAndInsertIntoParent<FunctionInfo const&>(*up) };
}

auto
Serializer::
buildInfo(
    TypedefDecl const* D) ->
        value_type
{
    TypedefInfo Info;

    bool IsInAnonymousNamespace = false;
    populateInfo(Info, D, std::move(jd_), IsInAnonymousNamespace, R_);
    if (!shouldSerializeInfo(PublicOnly, IsInAnonymousNamespace, D))
        return {};

    Info.DefLoc.emplace(LineNumber, File, IsFileInRootDir);
    Info.Underlying = getTypeInfoForType(D->getUnderlyingType());
    if (Info.Underlying.Type.Name.empty())
    {
        // Typedef for an unnamed type. This is like "typedef struct { } Foo;"
        // The record serializer explicitly checks for this syntax and constructs
        // a record with that name, so we don't want to emit a duplicate here.
        return {};
    }
    Info.IsUsing = false;

    // Info is wrapped in its parent scope so is returned in the second position.
    return { nullptr, MakeAndInsertIntoParent<TypedefInfo&&>(std::move(Info)) };
}

//------------------------------------------------

// A type alias is a C++ "using" declaration for a
// type. It gets mapped to a TypedefInfo with the
// IsUsing flag set.
auto
Serializer::
buildInfo(
    TypeAliasDecl const* D) ->
        value_type
{
    TypedefInfo Info;

    bool IsInAnonymousNamespace = false;
    populateInfo(Info, D, std::move(jd_), IsInAnonymousNamespace, R_);
    if (!shouldSerializeInfo(PublicOnly, IsInAnonymousNamespace, D))
        return {};

    Info.DefLoc.emplace(LineNumber, File, IsFileInRootDir);
    Info.Underlying = getTypeInfoForType(D->getUnderlyingType());
    Info.IsUsing = true;

    // Info is wrapped in its parent scope so is returned in the second position.
    return { nullptr, MakeAndInsertIntoParent<TypedefInfo&&>(std::move(Info)) };
}

auto
Serializer::
buildInfo(
    EnumDecl const* D) ->
        value_type
{
    EnumInfo Enum;
    bool IsInAnonymousNamespace = false;
    populateSymbolInfo(Enum, D, std::move(jd_), LineNumber, File, IsFileInRootDir,
        IsInAnonymousNamespace, R_);
    if (!shouldSerializeInfo(PublicOnly, IsInAnonymousNamespace, D))
        return {};

    Enum.Scoped = D->isScoped();
    if (D->isFixed())
    {
        auto Name = D->getIntegerType().getAsString();
        Enum.BaseType = TypeInfo(Name, Name);
    }
    parseEnumerators(Enum, D);

    // Info is wrapped in its parent scope so is returned in the second position.
    return { nullptr, MakeAndInsertIntoParent<EnumInfo&&>(std::move(Enum)) };
}

} // mrdox
} // clang
