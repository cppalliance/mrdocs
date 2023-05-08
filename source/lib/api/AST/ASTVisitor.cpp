//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "ASTVisitor.hpp"
#include "Bitcode.hpp"
#include "Commands.hpp"
#include "ParseJavadoc.hpp"
#include "api/ConfigImpl.hpp"
#include "api/Metadata/FunctionKind.hpp"
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
SymbolID
ASTVisitor::
getSymbolID(
    Decl const* D)
{
    llvm::SmallString<128> USR;
    if(index::generateUSRForDecl(D, USR))
        return SymbolID();
    return llvm::SHA1::hash(arrayRefFromStringRef(USR));
}

//------------------------------------------------

bool
ASTVisitor::
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

void
ASTVisitor::
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
        parent = getSymbolID(N);
    }
    else if(auto const* N = dyn_cast<RecordDecl>(DC))
    {
        parent = getSymbolID(N);
    }
    else if(auto const* N = dyn_cast<FunctionDecl>(DC))
    {
        parent = getSymbolID(N);
    }
    else if(auto const* N = dyn_cast<EnumDecl>(DC))
    {
        parent = getSymbolID(N);
    }
    else
    {
        Assert(false);
    }
    (void)isParentAnonymous;
}

void
ASTVisitor::
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
                getSymbolID(N),
                Namespace,
                InfoType::IT_namespace);
        }
        // for an explicit specialization of member of an implicit specialization,
        // treat it as-if it was declared in the primary class template
        else if(auto const* N = dyn_cast<ClassTemplateSpecializationDecl>(DC); N && 
            N->getSpecializationKind() == TemplateSpecializationKind::TSK_ImplicitInstantiation)
        {
            ClassTemplateDecl* P = N->getSpecializedTemplate();
            if(P->isMemberSpecialization())
                P = P->getInstantiatedFromMemberTemplate();

            Namespaces.emplace_back(
                getSymbolID(P),
                P->getNameAsString(),
                InfoType::IT_record);
        }
        else if(auto const* N = dyn_cast<RecordDecl>(DC))
        {
            Namespaces.emplace_back(
                getSymbolID(N),
                N->getNameAsString(),
                InfoType::IT_record);
        }
        else if(auto const* N = dyn_cast<FunctionDecl>(DC))
        {
            Namespaces.emplace_back(
                getSymbolID(N),
                N->getNameAsString(),
                InfoType::IT_function);
        }
        else if(auto const* N = dyn_cast<EnumDecl>(DC))
        {
            Namespaces.emplace_back(
                getSymbolID(N),
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

std::string
ASTVisitor::
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

std::string
ASTVisitor::
getTypeAsString(
    QualType T)
{
    return T.getAsString(astContext_->getPrintingPolicy());
}

Access
ASTVisitor::
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

TagDecl*
ASTVisitor::
getTagDeclForType(
    QualType T)
{
    if(TagDecl const* D = T->getAsTagDecl())
        return D->getDefinition();
    return nullptr;
}

RecordDecl*
ASTVisitor::
getRecordDeclForType(
    QualType T)
{
    if(RecordDecl const* D = T->getAsRecordDecl())
        return D->getDefinition();
    return nullptr;
}

TypeInfo
ASTVisitor::
getTypeInfoForType(
    QualType T)
{
    TagDecl const* TD = getTagDeclForType(T);
    if(!TD)
        return TypeInfo(Reference(EmptySID,
            getTypeAsString(T)));
    InfoType IT;
    if(dyn_cast<EnumDecl>(TD))
        IT = InfoType::IT_enum;
    else if(dyn_cast<RecordDecl>(TD))
        IT = InfoType::IT_record;
    else
        IT = InfoType::IT_default;
    return TypeInfo(Reference(
        getSymbolID(TD), TD->getNameAsString(), IT));
}

void
ASTVisitor::
parseParameters(
    FunctionInfo& I,
    FunctionDecl const* D)
{
    for(const ParmVarDecl* P : D->parameters())
    {
        // KRYSTIAN NOTE: call getOriginalType instead
        // of getType if we want to preserve top-level
        // cv-qualfiers/array types/function types
        FieldTypeInfo& FieldInfo = I.Params.emplace_back(
            getTypeInfoForType(P->getType()),
            P->getNameAsString());
        FieldInfo.DefaultValue = getSourceCode(
            D, P->getDefaultArgRange());
    }
}

TParam
ASTVisitor::
buildTemplateParam(
    const NamedDecl* ND)
{
    // KRYSTIAN NOTE: Decl::isParameterPack  
    // returns true for function parameter packs
    TParam info(
        ND->getNameAsString(),
        ND->isTemplateParameterPack());

    if(const auto* TP = dyn_cast<
        TemplateTypeParmDecl>(ND))
    {
        auto& extinfo = info.emplace<
            TypeTParam>();
        if(TP->hasDefaultArgument())
        {
            extinfo.Default.emplace(getTypeInfoForType(
                TP->getDefaultArgument()));
        }
    }
    else if(const auto* TP = dyn_cast<
        NonTypeTemplateParmDecl>(ND))
    {
        auto& extinfo = info.emplace<
            NonTypeTParam>();
        extinfo.Type = getTypeInfoForType(
            TP->getType());
        if(TP->hasDefaultArgument())
        {
            extinfo.Default.emplace(getSourceCode(
                ND, TP->getDefaultArgumentLoc()));
        }
    }
    else if(const auto* TP = dyn_cast<
        TemplateTemplateParmDecl>(ND))
    {
        auto& extinfo = info.emplace<
            TemplateTParam>();
        const auto* NestedParamList = TP->getTemplateParameters();
        for(const NamedDecl* NND : *NestedParamList)
        {
            extinfo.Params.emplace_back(
                buildTemplateParam(NND));
        }
        if(TP->hasDefaultArgument())
        {
            extinfo.Default.emplace(getSourceCode(
                ND, TP->getDefaultArgumentLoc()));
        }
    }
    return info;
}

void
ASTVisitor::
buildTemplateArgs(
    TemplateInfo& I,
    ArrayRef<TemplateArgument> args)
{
    // TypePrinter generates an internal placeholder name (e.g. type-parameter-0-0)
    // for template type parameters used as arguments. it also cannonicalizes
    // types, which we do not want (although, PrintingPolicy has an option to change this).
    // thus, we use the template arguments as written.

    // KRYSTIAN NOTE: this can probably be changed to select
    // the argument as written when it is not dependent and is a type.
    // FIXME: constant folding behavior should be consistent with that of other
    // constructs, e.g. noexcept specifiers & explicit specifiers
    const auto& policy = astContext_->getPrintingPolicy();
    for(const TemplateArgument& arg : args)
    {
        std::string arg_str;
        llvm::raw_string_ostream stream(arg_str);
        arg.print(policy, stream, false);
        I.Args.emplace_back(std::move(arg_str));
    }
}

void
ASTVisitor::
buildTemplateArgs(
    TemplateInfo& I,
    ArrayRef<TemplateArgumentLoc> args)
{
    // TypePrinter generates an internal placeholder name (e.g. type-parameter-0-0)
    // for template type parameters used as arguments. it also cannonicalizes
    // types, which we do not want (although, PrintingPolicy has an option to change this).
    // thus, we use the template arguments as written.

    // KRYSTIAN NOTE: this can probably be changed to select
    // the argument as written when it is not dependent and is a type.
    // FIXME: constant folding behavior should be consistent with that of other
    // constructs, e.g. noexcept specifiers & explicit specifiers
    const auto& policy = astContext_->getPrintingPolicy();
    for(const TemplateArgumentLoc& arg : args)
    {
        std::string arg_str;
        llvm::raw_string_ostream stream(arg_str);
        arg.getArgument().print(policy, stream, false);
        I.Args.emplace_back(std::move(arg_str));
    }
}

void
ASTVisitor::
parseTemplateArgs(
    llvm::Optional<TemplateInfo>& I,
    const ClassTemplateSpecializationDecl* spec)
{
    if(! I)
        I.emplace();

    // ID of the primary template
    if(ClassTemplateDecl* primary = spec->getSpecializedTemplate())
    {
        if(primary->isMemberSpecialization())
            primary = primary->getInstantiatedFromMemberTemplate();
        I->Primary.emplace(getSymbolID(primary));
    }

    const TypeSourceInfo* tsi = spec->getTypeAsWritten();
    // type source information *should* be non-null
    Assert(tsi);
    auto args = tsi->getType()->getAs<
        TemplateSpecializationType>()->template_arguments();
    buildTemplateArgs(*I, args);
}

void
ASTVisitor::
parseTemplateArgs(
    llvm::Optional<TemplateInfo>& I,
    const FunctionTemplateSpecializationInfo* spec)
{
    if(! I)
        I.emplace();

    // ID of the primary template
    // KRYSTIAN NOTE: do we need to check I->Primary.has_value()?
    if(FunctionTemplateDecl* primary = spec->getTemplate())
    {
        if(primary->isMemberSpecialization())
            primary = primary->getInstantiatedFromMemberTemplate();
        I->Primary.emplace(getSymbolID(primary));
    }

    auto args = spec->TemplateArguments->asArray();
    buildTemplateArgs(*I, args);
}

void
ASTVisitor::
parseTemplateArgs(
    llvm::Optional<TemplateInfo>& I,
    const ClassScopeFunctionSpecializationDecl* spec)
{
    if(! I)
        I.emplace();
    // KRYSTIAN NOTE: we have no way to get the ID of the primary template.
    // in the future, we could use name lookup to find matching declarations
    auto args = spec->getTemplateArgsAsWritten()->arguments();
    buildTemplateArgs(*I, args);
}

void
ASTVisitor::
parseTemplateParams(
    llvm::Optional<TemplateInfo>& TemplateInfo,
    const Decl* D)
{
    if(TemplateParameterList const* ParamList =
        D->getDescribedTemplateParams())
    {
        if(! TemplateInfo)
        {
            TemplateInfo.emplace();
        }
        for(const NamedDecl* ND : *ParamList)
        {
            TemplateInfo->Params.emplace_back(
                buildTemplateParam(ND));
        }
    }
}

void
ASTVisitor::
applyDecayToParameters(
    ASTContext* context,
    FunctionDecl* D)
{
    // apply the type adjustments specified in [dcl.fct] p5
    // to ensure that the USR of the corresponding function matches
    // other declarations of the function that have parameters declared
    // with different top-level cv-qualifiers.
    // this needs to be done prior to USR generation for the function
    for(ParmVarDecl* P : D->parameters())
        P->setType(context->getSignatureParameterType(P->getType()));
}

void
ASTVisitor::
parseRawComment(
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

void
ASTVisitor::
getMemberTypeInfo(
    MemberTypeInfo& I,
    FieldDecl const* D,
    Reporter& R)
{
    Assert(D && "Expect non-null FieldDecl in getMemberTypeInfo");
    parseRawComment(I.javadoc, D, R);

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

void
ASTVisitor::
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

void
ASTVisitor::
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
    parseRawComment(I.javadoc, D, R_);
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
        // KRYSTIAN NOTE: is this right? a class with a single
        // virtual base would be ignored here with ! config_.includePrivate
        if(isVirtual && ! config_.includePrivate)
            continue;
        if(auto const* Ty = B.getType()->getAs<TemplateSpecializationType>())
        {
            TemplateDecl const* TD = Ty->getTemplateName().getAsTemplateDecl();
            I.Bases.emplace_back(
                getSymbolID(TD),
                getTypeAsString(B.getType()),
                getAccessFromSpecifier(B.getAccessSpecifier()),
                isVirtual);
        }
        else if(RecordDecl const* P = getRecordDeclForType(B.getType()))
        {
            I.Bases.emplace_back(
                getSymbolID(P),
                P->getNameAsString(),
                getAccessFromSpecifier(B.getAccessSpecifier()),
                isVirtual);
        }
        else
        {
            I.Bases.emplace_back(
                EmptySID,
                getTypeAsString(B.getType()),
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
    FunctionInfo& I, DeclTy* D, char const* name)
{
    // adjust parameter types
    applyDecayToParameters(astContext_, D);
    if(! extractInfo(I, D))
        return false;
    if(name)
        I.Name = name;
    LineNumber = getLine(D);
    if(D->isThisDeclarationADefinition())
        I.DefLoc.emplace(LineNumber, File, IsFileInRootDir);
    else
        I.Loc.emplace_back(LineNumber, File, IsFileInRootDir);
    QualType const qt = D->getReturnType();
    std::string s = getTypeAsString(qt);
    I.ReturnType = getTypeInfoForType(qt);
    parseParameters(I, D);

    if(const auto* ftsi = D->getTemplateSpecializationInfo())
        parseTemplateArgs(I.Template, ftsi);

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

    {
        auto OOK = D->getOverloadedOperator();
        if(OOK != OverloadedOperatorKind::OO_None)
        {
            I.specs1.functionKind = getFunctionKind(OOK);
        }

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
    }

    //
    // CXXConstructorDecl
    //
    if constexpr(std::derived_from<DeclTy, CXXConstructorDecl>)
    {
        I.specs1.isExplicit = D->getExplicitSpecifier().isSpecified();
    }

    //
    // CXXConversionDecl
    //
    if constexpr(std::derived_from<DeclTy, CXXConversionDecl>)
    {
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
constructRecord(
    RecordInfo& I,
    CXXRecordDecl* D)
{
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
        auto Name = getTypeAsString(D->getIntegerType());
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
    static_cast<TypeInfo&>(I) = getTypeInfoForType(
        D->getTypeSourceInfo()->getType());
    I.specs.storageClass = D->getStorageClass();
    insertBitcode(ex_, writeBitcode(I));
    insertBitcode(ex_, writeParent(I, D->getAccess()));
}

template<class DeclTy>
void
ASTVisitor::
buildFunction(
    FunctionInfo& I,
    DeclTy* D,
    char const* name)
{
    if(! constructFunction(I, D, name))
        return;

    insertBitcode(ex_, writeBitcode(I));
    insertBitcode(ex_, writeParent(I, 
        std::derived_from<DeclTy, CXXMethodDecl> ? 
            D->getAccess() : AccessSpecifier::AS_none));
}

template<class DeclTy>
void
ASTVisitor::
buildFunction(
    DeclTy* D,
    char const* name)
{
    if(! shouldExtract(D))
        return;
    FunctionInfo I;
    buildFunction(I, D, name);
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
    I.Underlying = getTypeInfoForType(
        D->getUnderlyingType());
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
TraverseNamespaceDecl(
    NamespaceDecl* D)
{
    buildNamespace(D);

    for(auto* child : D->decls())
        TraverseDecl(child);
    return true;
}

bool
ASTVisitor::
TraverseCXXRecordDecl(
    CXXRecordDecl* D)
{
    if(! shouldExtract(D))
        return true;
    
    RecordInfo I;
    constructRecord(I, D);
    
    for(auto* child : D->decls())
        if(! TraverseDecl(child))
            return false;
    return true;
}

bool
ASTVisitor::
TraverseCXXMethodDecl(
    CXXMethodDecl* D)
{
    buildFunction(D);
    return true;
}

bool
ASTVisitor::
TraverseCXXDestructorDecl(
    CXXDestructorDecl* D)
{
    buildFunction(D);
    return true;
}

bool
ASTVisitor::
TraverseCXXConstructorDecl(
    CXXConstructorDecl* D)
{
    /*
    auto s = D->getParent()->getName();
    std::string s;
    DeclarationName DN = D->getDeclName();
    if(DN)
        s = DN.getAsString();
    */
    buildFunction(D);
    return true;
}

bool
ASTVisitor::
TraverseCXXConversionDecl(
    CXXConversionDecl* D)
{
    buildFunction(D);
    return true;
}

bool
ASTVisitor::
TraverseCXXDeductionGuideDecl(
    CXXDeductionGuideDecl* D)
{
    buildFunction(D);
    return true;
}

bool
ASTVisitor::
TraverseFunctionDecl(
    FunctionDecl* D)
{
    buildFunction(D);
    return true;
}

bool
ASTVisitor::
TraverseFriendDecl(
    FriendDecl* D)
{
    buildFriend(D);
    return true;
}

bool
ASTVisitor::
TraverseTypeAliasDecl(
    TypeAliasDecl* D)
{
    buildTypedef(D);
    return true;
}

bool
ASTVisitor::
TraverseTypedefDecl(
    TypedefDecl* D)
{
    buildTypedef(D);
    return true;
}

bool
ASTVisitor::
TraverseEnumDecl(
    EnumDecl* D)
{
    buildEnum(D);
    return true;
}

bool
ASTVisitor::
TraverseVarDecl(
    VarDecl* D)
{
    buildVar(D);
    return true;
}

bool
ASTVisitor::
TraverseClassTemplateDecl(
    ClassTemplateDecl* D)
{
    CXXRecordDecl* RD = D->getTemplatedDecl();
    if(! shouldExtract(RD))
        return true;
    RecordInfo I;

    parseTemplateParams(I.Template, RD);
    constructRecord(I, RD);

    for(auto* child : RD->decls())
        if(! TraverseDecl(child))
            return false;
    return true;
}

bool
ASTVisitor::
TraverseClassTemplateSpecializationDecl(
    ClassTemplateSpecializationDecl* D)
{
    CXXRecordDecl* RD = cast<CXXRecordDecl>(D); 
    if(! shouldExtract(RD))
        return true;
    RecordInfo I;

    parseTemplateParams(I.Template, RD);
    parseTemplateArgs(I.Template, D);
    constructRecord(I, RD);
    
    for(auto* child : RD->decls())
        if(! TraverseDecl(child))
            return false;
    return true;
}

bool
ASTVisitor::
TraverseClassTemplatePartialSpecializationDecl(
    ClassTemplatePartialSpecializationDecl* D)
{
    // without this function, we would only traverse 
    // explicit specialization declarations
    return TraverseClassTemplateSpecializationDecl(D);
}

bool
ASTVisitor::
TraverseFunctionTemplateDecl(
    FunctionTemplateDecl* D)
{
    FunctionDecl* FD = D->getTemplatedDecl();
    // check whether to extract using the templated declaration.
    // this is done because the template-head may be implicit 
    // (e.g. for an abbreviated function template with no template-head)
    if(! shouldExtract(FD))
        return true;
    FunctionInfo I;

    parseTemplateParams(I.Template, FD);

    switch(FD->getKind())
    {
    case Decl::Kind::Function:
        buildFunction(I, FD);
        break;
    case Decl::Kind::CXXMethod:
        buildFunction(I, cast<CXXMethodDecl>(FD));
        break;
    case Decl::Kind::CXXConstructor:
        buildFunction(I, cast<CXXConstructorDecl>(FD));
        break;
    case Decl::Kind::CXXConversion:
        buildFunction(I, cast<CXXConversionDecl>(FD));
        break;
    case Decl::Kind::CXXDeductionGuide:
        buildFunction(I, cast<CXXDeductionGuideDecl>(FD));
        break;
    case Decl::Kind::CXXDestructor:
        Assert(! "a destructor cannot be a template");
        break;
    default:
        Assert(! "unhandled kind for FunctionDecl");
        break;
    }
    // we don't care about any block scope declarations, 
    // nor do we need to traverse the FunctionDecl, so just return
    return true;
}

bool
ASTVisitor::
TraverseClassScopeFunctionSpecializationDecl(
    ClassScopeFunctionSpecializationDecl* D)
{
    if(! shouldExtract(D))
        return true;
    FunctionInfo I;

    parseTemplateArgs(I.Template, D);

    CXXMethodDecl* MD = D->getSpecialization();
    buildFunction(I, MD);
    
    return true;
}

} // mrdox
} // clang
