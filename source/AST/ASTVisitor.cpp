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
#include "ASTVisitorHelpers.hpp"
#include "Bitcode.hpp"
#include "ParseJavadoc.hpp"
#include "ConfigImpl.hpp"
#include "Support/Path.hpp"
#include "Support/Debug.hpp"
#include <mrdox/Metadata.hpp>
#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclFriend.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Index/USRGeneration.h>
#include <clang/Lex/Lexer.h>
#include <llvm/ADT/Hashing.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/SHA1.h>
#include <cassert>
#include <ranges>

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
    clang::CompilerInstance& compiler) noexcept
    : ex_(ex)
    , config_(config)
    , IsFileInRootDir_(true)
    , compiler_(compiler)
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
bool
ASTVisitor::
extractSymbolID(
    const Decl* D,
    SymbolID& id)
{
    usr_.clear();
    if(index::generateUSRForDecl(D, usr_))
        return false;
    id = SymbolID(llvm::SHA1::hash(
        arrayRefFromStringRef(usr_)).data());
    return true;
}

SymbolID
ASTVisitor::
extractSymbolID(
    const Decl* D)
{
    SymbolID id = SymbolID::zero;
    extractSymbolID(D, id);
    return id;
}

bool
ASTVisitor::
shouldSerializeInfo(
    const NamedDecl* D) noexcept
{
    // KRYSTIAN FIXME: getting the access of a members
    // is not as simple as calling Decl::getAccessUnsafe.
    // specifically, templates may not have
    // their access set until they are actually instantiated.
    return true;
#if 0
    if(config_.includePrivate)
        return true;
    if(IsOrIsInAnonymousNamespace)
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
#endif
}

//------------------------------------------------

int
ASTVisitor::
getLine(
    const NamedDecl* D) const
{
    return sourceManager_->getPresumedLoc(
        D->getBeginLoc()).getLine();
}

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

CXXRecordDecl*
ASTVisitor::
getCXXRecordDeclForType(
    QualType T)
{
    if(CXXRecordDecl const* D = T->getAsCXXRecordDecl())
        return D->getDefinition();
    return nullptr;
}

TypeInfo
ASTVisitor::
getTypeInfoForType(
    QualType T)
{
    SymbolID id = SymbolID::zero;
    if(const TagDecl* TD = getTagDeclForType(T))
    {
        InfoKind kind;
        if(isa<EnumDecl>(TD))
            kind = InfoKind::Enum;
        else if(isa<CXXRecordDecl>(TD))
            kind = InfoKind::Record;
        else
            kind = InfoKind::Default;
        extractSymbolID(TD, id);
        return TypeInfo(Reference(
            id, TD->getNameAsString(), kind));
    }
    return TypeInfo(Reference(id,
        getTypeAsString(T)));
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
        I.Params.emplace_back(
            getTypeInfoForType(P->getType()),
            P->getNameAsString(),
            getSourceCode(D, P->getDefaultArgRange()));
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

template<typename Range>
void
ASTVisitor::
buildTemplateArgs(
    std::vector<TArg>& result,
    Range&& range)
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
    for(const TemplateArgument& arg : range)
    {
        std::string arg_str;
        llvm::raw_string_ostream stream(arg_str);
        arg.print(policy, stream, false);
        result.emplace_back(std::move(arg_str));
    }
}

void
ASTVisitor::
parseTemplateArgs(
    TemplateInfo& I,
    const ClassTemplateSpecializationDecl* spec)
{
    // KRYSTIAN FIXME: should this use getTemplateInstantiationPattern?
    // ID of the primary template
    if(ClassTemplateDecl* primary = spec->getSpecializedTemplate())
    {
        if(auto* MT = primary->getInstantiatedFromMemberTemplate())
            primary = MT;
        extractSymbolID(primary, I.Primary.emplace());
    }
    // KRYSTIAN NOTE: when this is a partial specialization, we could use
    // ClassTemplatePartialSpecializationDecl::getTemplateArgsAsWritten
    const TypeSourceInfo* type_written = spec->getTypeAsWritten();
    // if the type as written is nullptr (it should never be), bail
    if(! type_written)
        return;
    auto args = type_written->getType()->getAs<
        TemplateSpecializationType>()->template_arguments();
    buildTemplateArgs(I.Args, args);
}

void
ASTVisitor::
parseTemplateArgs(
    TemplateInfo& I,
    const VarTemplateSpecializationDecl* spec)
{
    // KRYSTIAN FIXME: should this use getTemplateInstantiationPattern?
    // ID of the primary template
    if(VarTemplateDecl* primary = spec->getSpecializedTemplate())
    {
        if(auto* MT = primary->getInstantiatedFromMemberTemplate())
            primary = MT;
        // unlike function and class templates, the USR generated
        // for variable templates differs from that of the VarDecl
        // returned by getTemplatedDecl. this might be a clang bug.
        // the USR of the templated VarDecl seems to be the correct one.
        extractSymbolID(primary->getTemplatedDecl(), I.Primary.emplace());
    }
    const ASTTemplateArgumentListInfo* args_written = nullptr;
    // getTemplateArgsInfo returns nullptr for partial specializations,
    // so we use getTemplateArgsAsWritten if this is a partial specialization
    if(auto* partial = dyn_cast<VarTemplatePartialSpecializationDecl>(spec))
        args_written = partial->getTemplateArgsAsWritten();
    else
        args_written = spec->getTemplateArgsInfo();
    if(! args_written)
        return;
    auto args = args_written->arguments();
    buildTemplateArgs(I.Args, std::views::transform(
        args, [](auto& x) -> auto& { return x.getArgument(); }));
}

void
ASTVisitor::
parseTemplateArgs(
    TemplateInfo& I,
    const FunctionTemplateSpecializationInfo* spec)
{
    // KRYSTIAN FIXME: should this use getTemplateInstantiationPattern?
    // ID of the primary template
    // KRYSTIAN NOTE: do we need to check I->Primary.has_value()?
    if(FunctionTemplateDecl* primary = spec->getTemplate())
    {
        if(auto* MT = primary->getInstantiatedFromMemberTemplate())
            primary = MT;
        extractSymbolID(primary, I.Primary.emplace());
    }
    // TemplateArguments is used instead of TemplateArgumentsAsWritten
    // because explicit specializations of function templates may have
    // template arguments deduced from their return type and parameters
    if(auto* args = spec->TemplateArguments)
        buildTemplateArgs(I.Args, args->asArray());
}

void
ASTVisitor::
parseTemplateArgs(
    TemplateInfo& I,
    const ClassScopeFunctionSpecializationDecl* spec)
{
    // if(! spec->hasExplicitTemplateArgs())
    //     return;
    // KRYSTIAN NOTE: we have no way to get the ID of the primary template;
    // it is unknown what function template this will be an explicit
    // specialization of until the enclosing class template is instantiated.
    // this also means that we can only extract the explicit template arguments.
    // in the future, we could use name lookup to find matching declarations
    if(auto* args_written = spec->getTemplateArgsAsWritten())
    {
        auto args = args_written->arguments();
        buildTemplateArgs(I.Args, std::views::transform(
            args, [](auto& x) -> auto& { return x.getArgument(); }));
    }
}

void
ASTVisitor::
parseTemplateParams(
    TemplateInfo& I,
    const Decl* D)
{
    if(const TemplateParameterList* ParamList =
        D->getDescribedTemplateParams())
    {
        for(const NamedDecl* ND : *ParamList)
        {
            I.Params.emplace_back(
                buildTemplateParam(ND));
        }
    }
}

void
ASTVisitor::
applyDecayToParameters(
    FunctionDecl* D)
{
    // apply the type adjustments specified in [dcl.fct] p5
    // to ensure that the USR of the corresponding function matches
    // other declarations of the function that have parameters declared
    // with different top-level cv-qualifiers.
    // this needs to be done prior to USR generation for the function
    for(ParmVarDecl* P : D->parameters())
        P->setType(astContext_->getSignatureParameterType(P->getType()));
}

void
ASTVisitor::
parseRawComment(
    std::unique_ptr<Javadoc>& javadoc,
    Decl const* D)
{
    // VFALCO investigate whether we can use
    // ASTContext::getCommentForDecl instead
    RawComment* RC =
        D->getASTContext().getRawCommentForDeclNoCache(D);
    if(RC)
    {
        RC->setAttached();
        javadoc = std::make_unique<Javadoc>(
            parseJavadoc(RC, D));
    }
    else
    {
        javadoc.reset();
    }
}

//------------------------------------------------

template<class Child>
requires std::derived_from<Child, Info>
static
Bitcode
writeParent(
    Child const& I,
    AccessSpecifier access = AccessSpecifier::AS_none)
{
    Assert(! I.Namespace.empty());
    Access access_;
    switch(access)
    {
    // namespace scope declaration
    case AccessSpecifier::AS_none:
    {
        NamespaceInfo P(I.Namespace.front());
        insertChild<Child>(P.Children,
            I.id, I.Name, Child::kind_id);
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
    // and insert the child as a MemberRef.
    // Then return the parent as a serialized bitcode.
    RecordInfo P(I.Namespace.front());
    if constexpr(std::is_same_v<Child, SpecializationInfo>)
        insertChild<Child>(P.Members, I.id);
    else
        insertChild<Child>(P.Members, I.id, access_);

    return writeBitcode(P);
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
        I.Members.emplace_back(E->getNameAsString(), ValueStr.str(), ValueExpr);
    }
}

//------------------------------------------------

// This also sets IsFileInRootDir
bool
ASTVisitor::
shouldExtract(
    const Decl* D)
{
    namespace path = llvm::sys::path;

    // skip system header
    if(sourceManager_->isInSystemHeader(D->getLocation()))
        return false;

    // we should never visit block scope declarations
    Assert(! D->getParentFunctionOrMethod());

    const PresumedLoc loc =
        sourceManager_->getPresumedLoc(D->getBeginLoc());

    auto [it, inserted] = fileFilter_.emplace(
        loc.getIncludeLoc().getRawEncoding(),
        FileFilter());

    FileFilter& ff = it->second;
    File_ = files::makePosixStyle(loc.getFilename());

    // file has not been previously visited
    if(inserted)
        ff.include = config_.shouldVisitFile(File_, ff.prefix);

    // don't extract if the declaration is in a file
    // that should not be visited
    if(! ff.include)
        return false;
    // VFALCO we could assert that the prefix
    //        matches and just lop off the
    //        first ff.prefix.size() characters.
    path::replace_path_prefix(File_, ff.prefix, "");

    IsFileInRootDir_ = true;

    return true;
}

bool
ASTVisitor::
extractInfo(
    Info& I,
    const NamedDecl* D)
{
    if(! shouldSerializeInfo(D))
        return false;
    if(! extractSymbolID(D, I.id))
        return false;
    I.Name = D->getNameAsString();
    parseRawComment(I.javadoc, D);
    return true;
}

//------------------------------------------------

bool
ASTVisitor::
getParentNamespaces(
    std::vector<SymbolID>& Namespaces,
    const Decl* D)
{
    bool member_specialization = false;
    const Decl* child = D;
    const DeclContext* parent_context = child->getDeclContext();
    do
    {
        const Decl* parent = cast<Decl>(parent_context);

        SymbolID id = SymbolID::zero;
        switch(parent_context->getDeclKind())
        {
        default:
            // we consider all other DeclContexts to be "transparent"
            // and do not include them in the list of parents.
            continue;
        // the TranslationUnit DeclContext
        // is the global namespace; use SymbolID::zero
        case Decl::TranslationUnit:
            break;
        // special case for an explicit specializations of
        // a member of an implicit instantiation.
        case Decl::ClassTemplateSpecialization:
        case Decl::ClassTemplatePartialSpecialization:
        if(const auto* S = dyn_cast<ClassTemplateSpecializationDecl>(parent_context);
            S && S->getSpecializationKind() == TSK_ImplicitInstantiation)
        {
            member_specialization = true;
            // KRYSTIAN FIXME: i'm pretty sure DeclContext::getDeclKind()
            // will never be Decl::ClassTemplatePartialSpecialization for
            // implicit instantiations; instead, the ClassTemplatePartialSpecializationDecl
            // is accessible through S->getSpecializedTemplateOrPartial
            // if the implicit instantiation used a partially specialized template,
            Assert(parent_context->getDeclKind() !=
                Decl::ClassTemplatePartialSpecialization);

            SpecializationInfo I;
            buildSpecialization(I, S, child);
        }
        [[fallthrough]];
        // we should never encounter a Record
        // that is not a CXXRecord
        // case Decl::Record:
        case Decl::Namespace:
        case Decl::CXXRecord:
        case Decl::Enum:
        // we currently don't handle local classes,
        // but will at some point. only functions that may
        // be declared to return a placeholder for a deduced type
        // can return local classes, so ignore other function DeclContexts.
        // deduction guides, constructors, and destructors do not have
        // declared return types, so we do not need to consider them.
        case Decl::Function:
        case Decl::CXXMethod:
        case Decl::CXXConversion:
        {
            extractSymbolID(parent, id);
            break;
        }
        }
        Namespaces.emplace_back(id);
        child = parent;
    }
    while((parent_context = parent_context->getParent()));
    return member_specialization;
}

//------------------------------------------------

void
ASTVisitor::
buildSpecialization(
    SpecializationInfo& I,
    const ClassTemplateSpecializationDecl* P,
    const Decl* C)
{
    const CXXRecordDecl* RD =
        P->getTemplateInstantiationPattern();
    Assert(RD);

    extractSymbolID(P, I.id);
    extractSymbolID(RD, I.Primary);
    buildTemplateArgs(I.Args,
        P->getTemplateArgs().asArray());
    I.Name = RD->getNameAsString();

    SymbolID child = extractSymbolID(C);
    // KRYSTIAN TODO: properly extract the primary ID
    // of the specialized member
    I.Members.emplace_back(child, child);

    bool member_spec = getParentNamespaces(I.Namespace, P);

    insertBitcode(ex_, writeBitcode(I));
    if(! member_spec)
        insertBitcode(ex_, writeParent(I,
            P->getDeclContext()->isFileContext() ?
                AccessSpecifier::AS_none : AccessSpecifier::AS_public));
}

//------------------------------------------------

void
ASTVisitor::
extractBases(
    RecordInfo& I,
    CXXRecordDecl* D)
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

        SymbolID id = SymbolID::zero;
        if(auto const* Ty = B.getType()->getAs<TemplateSpecializationType>())
        {
            TemplateDecl const* TD = Ty->getTemplateName().getAsTemplateDecl();
            extractSymbolID(TD, id);
            I.Bases.emplace_back(
                id,
                getTypeAsString(B.getType()),
                getAccessFromSpecifier(B.getAccessSpecifier()),
                isVirtual);
        }
        else if(CXXRecordDecl const* P = getCXXRecordDeclForType(B.getType()))
        {
            extractSymbolID(P, id);
            I.Bases.emplace_back(
                id,
                P->getNameAsString(),
                getAccessFromSpecifier(B.getAccessSpecifier()),
                isVirtual);
        }
        else
        {
            I.Bases.emplace_back(
                id,
                getTypeAsString(B.getType()),
                getAccessFromSpecifier(B.getAccessSpecifier()),
                isVirtual);
        }
    }
}

//------------------------------------------------

void
ASTVisitor::
buildRecord(
    RecordInfo& I,
    AccessSpecifier A,
    CXXRecordDecl* D)
{
    if(! extractInfo(I, D))
        return;
    int line = getLine(D);
    if(D->isThisDeclarationADefinition())
        I.DefLoc.emplace(line, File_.str(), IsFileInRootDir_);
    else
        I.Loc.emplace_back(line, File_.str(), IsFileInRootDir_);

    switch(D->getTagKind())
    {
    case TagTypeKind::TTK_Struct:
        I.KeyKind = RecordKeyKind::Struct;
        break;
    case TagTypeKind::TTK_Class:
        I.KeyKind = RecordKeyKind::Class;
        break;
    case TagTypeKind::TTK_Union:
        I.KeyKind = RecordKeyKind::Union;
        break;
    default:
        llvm_unreachable("unsupported TagTypeKind");
    }

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

    bool member_spec = getParentNamespaces(I.Namespace, D);

    insertBitcode(ex_, writeBitcode(I));
    if(! member_spec)
        insertBitcode(ex_, writeParent(I, A));
}

//------------------------------------------------

template<class DeclTy>
bool
ASTVisitor::
constructFunction(
    FunctionInfo& I,
    AccessSpecifier A,
    DeclTy* D)
{
    // adjust parameter types
    applyDecayToParameters(D);
    if(! extractInfo(I, D))
        return false;
    // if(name)
    //     I.Name = name;
    int line = getLine(D);
    if(D->isThisDeclarationADefinition())
        I.DefLoc.emplace(line, File_.str(), IsFileInRootDir_);
    else
        I.Loc.emplace_back(line, File_.str(), IsFileInRootDir_);
    QualType qt = D->getReturnType();
    std::string s = getTypeAsString(qt);
    I.ReturnType = getTypeInfoForType(qt);
    parseParameters(I, D);

    if(const auto* ftsi = D->getTemplateSpecializationInfo())
    {
        if(! I.Template)
            I.Template = std::make_unique<TemplateInfo>();
        parseTemplateArgs(*I.Template, ftsi);
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
    NamespaceInfo& I,
    NamespaceDecl* D)
{
    if(! extractInfo(I, D))
        return;
    if(D->isAnonymousNamespace())
        I.Name = "@nonymous_namespace"; // VFALCO BAD!

    bool member_spec = getParentNamespaces(I.Namespace, D);

    insertBitcode(ex_, writeBitcode(I));
    if(! member_spec)
        insertBitcode(ex_, writeParent(I));
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
            if(! constructFunction(I, FD->getAccessUnsafe(), FD))
                return;
#if 0
            SymbolID id;
            getParent(id, D);
#else
            const DeclContext* DC = D->getDeclContext();
            const auto* RD = dyn_cast<CXXRecordDecl>(DC);
            // the semantic DeclContext of a FriendDecl must be a class
            Assert(RD);
            RecordInfo P;
            extractSymbolID(RD, P.id);

#if 0
            SymbolID id_;
            getParent(id_, D);
            Assert(id_ == id);
#endif
#endif
            P.Friends.emplace_back(I.id);
#if 0
            bool isInAnonymous;
            getParentNamespaces(P.Namespace, ND, isInAnonymous);
            Assert(isInAnonymous == ND->isInAnonymousNamespace());
#else
            getParentNamespaces(I.Namespace, FD);
            getParentNamespaces(P.Namespace, ND);
#endif
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
    EnumInfo& I,
    AccessSpecifier A,
    EnumDecl* D)
{
    if(! extractInfo(I, D))
        return;
    int line = getLine(D);
    if(D->isThisDeclarationADefinition())
        I.DefLoc.emplace(line, File_.str(), IsFileInRootDir_);
    else
        I.Loc.emplace_back(line, File_.str(), IsFileInRootDir_);
    I.Scoped = D->isScoped();
    if(D->isFixed())
    {
        auto Name = getTypeAsString(D->getIntegerType());
        I.BaseType = TypeInfo(Name);
    }
    parseEnumerators(I, D);

    bool member_spec = getParentNamespaces(I.Namespace, D);

    insertBitcode(ex_, writeBitcode(I));
    if(! member_spec)
        insertBitcode(ex_, writeParent(I, A));
}

void
ASTVisitor::
buildField(
    FieldInfo& I,
    AccessSpecifier A,
    FieldDecl* D)
{
    if(! extractInfo(I, D))
        return;
    int line = getLine(D);
    I.DefLoc.emplace(line, File_.str(), IsFileInRootDir_);

    I.Type = getTypeInfoForType(
        D->getTypeSourceInfo()->getType());

    I.specs.hasNoUniqueAddress = D->hasAttr<NoUniqueAddressAttr>();
    I.specs.isDeprecated = D->hasAttr<DeprecatedAttr>();
    // KRYSTIAN FIXME: isNodiscard should be isMaybeUnused
    I.specs.isNodiscard = D->hasAttr<UnusedAttr>();

    bool member_spec = getParentNamespaces(I.Namespace, D);

    insertBitcode(ex_, writeBitcode(I));
    if(! member_spec)
        insertBitcode(ex_, writeParent(I, A));
}

void
ASTVisitor::
buildVar(
    VarInfo& I,
    AccessSpecifier A,
    VarDecl* D)
{
    if(! extractInfo(I, D))
        return;
    int line = getLine(D);
    if(D->isThisDeclarationADefinition())
        I.DefLoc.emplace(line, File_.str(), IsFileInRootDir_);
    else
        I.Loc.emplace_back(line, File_.str(), IsFileInRootDir_);
    I.Type = getTypeInfoForType(
        D->getTypeSourceInfo()->getType());
    I.specs.storageClass = D->getStorageClass();

    bool member_spec = getParentNamespaces(I.Namespace, D);

    insertBitcode(ex_, writeBitcode(I));
    if(! member_spec)
        insertBitcode(ex_, writeParent(I, A));
}

template<class DeclTy>
void
ASTVisitor::
buildFunction(
    FunctionInfo& I,
    AccessSpecifier A,
    DeclTy* D)
{
    if(! constructFunction(I, A, D))
        return;

    bool member_spec = getParentNamespaces(I.Namespace, D);

    insertBitcode(ex_, writeBitcode(I));
    if(! member_spec)
        insertBitcode(ex_, writeParent(I, A));
}

template<class DeclTy>
void
ASTVisitor::
buildTypedef(
    TypedefInfo& I,
    AccessSpecifier A,
    DeclTy* D)
{
    if(! extractInfo(I, D))
        return;
    I.Underlying = getTypeInfoForType(
        D->getUnderlyingType());
    if(I.Underlying.Name.empty())
    {
        // Typedef for an unnamed type. This is like
        // "typedef struct { } Foo;". The record serializer
        // explicitly checks for this syntax and constructs
        // a record with that name, so we don't want to emit
        // a duplicate here.
        return;
    }

    int line = getLine(D);
    // D->isThisDeclarationADefinition(); // not available
    I.DefLoc.emplace(line, File_.str(), IsFileInRootDir_);
    // KRYSTIAN NOTE: IsUsing is set by TraverseTypeAlias
    // I.IsUsing = std::is_same_v<DeclTy, TypeAliasDecl>;

    bool member_spec = getParentNamespaces(I.Namespace, D);

    insertBitcode(ex_, writeBitcode(I));
    if(! member_spec)
        insertBitcode(ex_, writeParent(I, D->getAccess()));
}

//------------------------------------------------

bool
ASTVisitor::
Traverse(
    NamespaceDecl* D)
{
    if(! shouldExtract(D))
        return true;
    if(! config_.includeAnonymous &&
        D->isAnonymousNamespace())
        return true;

    NamespaceInfo I;
    buildNamespace(I, D);

    return TraverseContext(D);
}

bool
ASTVisitor::
Traverse(
    CXXRecordDecl* D,
    AccessSpecifier A,
    std::unique_ptr<TemplateInfo>&& Template = nullptr)
{
    if(! shouldExtract(D))
        return true;

    Assert(! D->getDeclContext()->isRecord() ||
        A != AccessSpecifier::AS_none);

    RecordInfo I(std::move(Template));
    buildRecord(I, A, D);

    return TraverseContext(D);
}

bool
ASTVisitor::
Traverse(
    TypedefDecl* D,
    AccessSpecifier A)
{
    if(! shouldExtract(D))
        return true;

    Assert(! D->getDeclContext()->isRecord() ||
        A != AccessSpecifier::AS_none);

    TypedefInfo I;
    buildTypedef(I, A, D);
    return true;
}

bool
ASTVisitor::
Traverse(
    TypeAliasDecl* D,
    AccessSpecifier A,
    std::unique_ptr<TemplateInfo>&& Template = nullptr)
{
    if(! shouldExtract(D))
        return true;

    Assert(! D->getDeclContext()->isRecord() ||
        A != AccessSpecifier::AS_none);

    TypedefInfo I(std::move(Template));
    I.IsUsing = true;

    buildTypedef(I, A, D);
    return true;
}

bool
ASTVisitor::
Traverse(
    VarDecl* D,
    AccessSpecifier A,
    std::unique_ptr<TemplateInfo>&& Template = nullptr)
{
    if(! shouldExtract(D))
        return true;

    Assert(! D->getDeclContext()->isRecord() ||
        A != AccessSpecifier::AS_none);

    VarInfo I(std::move(Template));
    buildVar(I, A, D);
    return true;
}

bool
ASTVisitor::
Traverse(
    FunctionDecl* D,
    AccessSpecifier A,
    std::unique_ptr<TemplateInfo>&& Template = nullptr)
{
    if(! shouldExtract(D))
        return true;

    Assert(! D->getDeclContext()->isRecord());
    Assert(A == AccessSpecifier::AS_none);

    FunctionInfo I(std::move(Template));
    buildFunction(I, A, D);

    return true;
}

bool
ASTVisitor::
Traverse(
    CXXMethodDecl* D,
    AccessSpecifier A,
    std::unique_ptr<TemplateInfo>&& Template = nullptr)
{
    if(! shouldExtract(D))
        return true;

    Assert(D->getDeclContext()->isRecord());
    Assert(A != AccessSpecifier::AS_none);

    FunctionInfo I(std::move(Template));
    buildFunction(I, A, D);
    return true;
}

bool
ASTVisitor::
Traverse(
    CXXConstructorDecl* D,
    AccessSpecifier A,
    std::unique_ptr<TemplateInfo>&& Template = nullptr)
{
    /*
    auto s = D->getParent()->getName();
    std::string s;
    DeclarationName DN = D->getDeclName();
    if(DN)
        s = DN.getAsString();
    */
    if(! shouldExtract(D))
        return true;

    Assert(D->getDeclContext()->isRecord());
    Assert(A != AccessSpecifier::AS_none);

    FunctionInfo I(std::move(Template));
    buildFunction(I, A, D);
    return true;
}

bool
ASTVisitor::
Traverse(
    CXXConversionDecl* D,
    AccessSpecifier A,
    std::unique_ptr<TemplateInfo>&& Template = nullptr)
{
    if(! shouldExtract(D))
        return true;

    Assert(D->getDeclContext()->isRecord());
    Assert(A != AccessSpecifier::AS_none);

    FunctionInfo I(std::move(Template));
    buildFunction(I, A, D);
    return true;
}

bool
ASTVisitor::
Traverse(
    CXXDeductionGuideDecl* D,
    AccessSpecifier A,
    std::unique_ptr<TemplateInfo>&& Template = nullptr)
{
    if(! shouldExtract(D))
        return true;

    FunctionInfo I(std::move(Template));
    buildFunction(I, A, D);
    return true;
}

bool
ASTVisitor::
Traverse(
    CXXDestructorDecl* D,
    AccessSpecifier A)
{
    if(! shouldExtract(D))
        return true;

    Assert(D->getDeclContext()->isRecord());
    Assert(A != AccessSpecifier::AS_none);

    FunctionInfo I;
    buildFunction(I, A, D);
    return true;
}

bool
ASTVisitor::
Traverse(
    FriendDecl* D)
{
    buildFriend(D);
    return true;
}

bool
ASTVisitor::
Traverse(
    EnumDecl* D,
    AccessSpecifier A)
{
    if(! shouldExtract(D))
        return true;

    Assert(! D->getDeclContext()->isRecord() ||
        A != AccessSpecifier::AS_none);

    EnumInfo I;
    buildEnum(I, A, D);
    return true;
}

bool
ASTVisitor::
Traverse(
    FieldDecl* D,
    AccessSpecifier A)
{
    if(! shouldExtract(D))
        return true;

    Assert(D->getDeclContext()->isRecord());
    Assert(A != AccessSpecifier::AS_none);

    FieldInfo I;
    buildField(I, A, D);
    return true;
}

bool
ASTVisitor::
Traverse(
    ClassTemplateDecl* D,
    AccessSpecifier A)
{
    CXXRecordDecl* RD = D->getTemplatedDecl();
    if(! shouldExtract(RD))
        return true;

    auto Template = std::make_unique<TemplateInfo>();
    parseTemplateParams(*Template, RD);

    return Traverse(RD, A, std::move(Template));
}

bool
ASTVisitor::
Traverse(
    ClassTemplateSpecializationDecl* D)
{
    CXXRecordDecl* RD = D;
    if(! shouldExtract(RD))
        return true;

    auto Template = std::make_unique<TemplateInfo>();
    parseTemplateParams(*Template, RD);
    parseTemplateArgs(*Template, D);

    // determine the access from the primary template
    return Traverse(RD,
        D->getSpecializedTemplate()->getAccessUnsafe(),
        std::move(Template));
}

bool
ASTVisitor::
Traverse(
    VarTemplateDecl* D,
    AccessSpecifier A)
{
    VarDecl* VD = D->getTemplatedDecl();
    if(! shouldExtract(VD))
        return true;

    auto Template = std::make_unique<TemplateInfo>();
    parseTemplateParams(*Template, VD);

    return Traverse(VD, A, std::move(Template));
}

bool
ASTVisitor::
Traverse(
    VarTemplateSpecializationDecl* D)
{
    VarDecl* VD = D;
    if(! shouldExtract(VD))
        return true;

    auto Template = std::make_unique<TemplateInfo>();
    parseTemplateParams(*Template, VD);
    parseTemplateArgs(*Template, D);

    return Traverse(VD,
        D->getSpecializedTemplate()->getAccessUnsafe(),
        std::move(Template));
}

bool
ASTVisitor::
Traverse(
    FunctionTemplateDecl* D,
    AccessSpecifier A)
{
    FunctionDecl* FD = D->getTemplatedDecl();
    // check whether to extract using the templated declaration.
    // this is done because the template-head may be implicit
    // (e.g. for an abbreviated function template with no template-head)
    if(! shouldExtract(FD))
        return true;
    auto Template = std::make_unique<TemplateInfo>();
    parseTemplateParams(*Template, FD);

    // traverse the templated declaration according to its kind
    return TraverseDecl(FD, std::move(Template));
}

bool
ASTVisitor::
Traverse(
    ClassScopeFunctionSpecializationDecl* D)
{
    if(! shouldExtract(D))
        return true;

    /* For class scope explicit specializations of member function templates which
       are members of class templates, it is impossible to know what the
       primary template is until the enclosing class template is instantiated.
       while such declarations are valid C++ (see CWG 727 and [temp.expl.spec] p3),
       GCC does not consider them to be valid. Consequently, we do not extract the SymbolID
       of the primary template. In the future, we could take a best-effort approach to find
       the primary template, but this is only possible when none of the candidates are dependent
       upon a template parameter of the enclosing class template.
    */
    auto Template = std::make_unique<TemplateInfo>();
    parseTemplateArgs(*Template, D);

    CXXMethodDecl* MD = D->getSpecialization();

    // since the templated CXXMethodDecl may be a constructor
    // or conversion function, call TraverseDecl to ensure that
    // we call traverse for the dynamic type of the CXXMethodDecl
    return TraverseDecl(MD, std::move(Template));
}

bool
ASTVisitor::
Traverse(
    TypeAliasTemplateDecl* D,
    AccessSpecifier A)
{
    TypeAliasDecl* AD = D->getTemplatedDecl();
    if(! shouldExtract(AD))
        return true;

    auto Template = std::make_unique<TemplateInfo>();
    parseTemplateParams(*Template, AD);

    return Traverse(AD, A, std::move(Template));
}

template<typename... Args>
auto
ASTVisitor::
Traverse(Args&&...)
{
    llvm_unreachable("no matching Traverse overload found");
}

//------------------------------------------------

template<typename... Args>
bool
ASTVisitor::
TraverseDecl(
    Decl* D,
    Args&&... args)
{
    Assert(D);
    if(D->isInvalidDecl() || D->isImplicit())
        return true;

    AccessSpecifier access =
        D->getAccessUnsafe();

    switch(D->getKind())
    {
    case Decl::Namespace:
        Traverse(static_cast<
            NamespaceDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::CXXRecord:
        Traverse(static_cast<
            CXXRecordDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::CXXMethod:
        Traverse(static_cast<
            CXXMethodDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::CXXConstructor:
        Traverse(static_cast<
            CXXConstructorDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::CXXConversion:
        Traverse(static_cast<
            CXXConversionDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::CXXDestructor:
        Traverse(static_cast<
            CXXDestructorDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::CXXDeductionGuide:
        Traverse(static_cast<
            CXXDeductionGuideDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::Function:
        Traverse(static_cast<
            FunctionDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::Friend:
        Traverse(static_cast<
            FriendDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::TypeAlias:
        Traverse(static_cast<
            TypeAliasDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::Typedef:
        Traverse(static_cast<
            TypedefDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::Enum:
        Traverse(static_cast<
            EnumDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::Field:
        Traverse(static_cast<
            FieldDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::Var:
        Traverse(static_cast<
            VarDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::ClassTemplate:
        Traverse(static_cast<
            ClassTemplateDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::ClassTemplatePartialSpecialization:
    case Decl::ClassTemplateSpecialization:
        Traverse(static_cast<
            ClassTemplateSpecializationDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::VarTemplate:
        Traverse(static_cast<
            VarTemplateDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::VarTemplatePartialSpecialization:
    case Decl::VarTemplateSpecialization:
        Traverse(static_cast<
            VarTemplateSpecializationDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::FunctionTemplate:
        Traverse(static_cast<
            FunctionTemplateDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::ClassScopeFunctionSpecialization:
        Traverse(static_cast<
            ClassScopeFunctionSpecializationDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::TypeAliasTemplate:
        Traverse(static_cast<
            TypeAliasTemplateDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    default:
        // for declarations we don't explicitly handle, traverse the children
        // if it has any (e.g. LinkageSpecDecl, ExportDecl, ExternCContextDecl).
        if(auto* DC = dyn_cast<DeclContext>(D))
            TraverseContext(DC);
        break;
    }

    return true;
}

bool
ASTVisitor::
TraverseContext(
    DeclContext* D)
{
    Assert(D);
    for(auto* C : D->decls())
        if(! TraverseDecl(C))
            return false;
    return true;
}

// An instance of Visitor runs on one translation unit.
void
ASTVisitor::
HandleTranslationUnit(
    ASTContext& Context)
{
    // the ASTContext and Sema better be the same
    // as those set by Initialize and InitializeSema
    Assert(astContext_ == &Context);
    Assert(sema_);

    // Install handlers for our custom commands
    initCustomCommentCommands(Context);

    std::optional<llvm::StringRef> filePath =
        Context.getSourceManager().getNonBuiltinFilenameForID(
            Context.getSourceManager().getMainFileID());
    if(! filePath)
        return;

    // Filter out TUs we don't care about
    File_ = *filePath;
    convert_to_slash(File_);
    if(! config_.shouldVisitTU(File_))
        return;

    TranslationUnitDecl* TU =
        Context.getTranslationUnitDecl();
    // the traversal scope should *only* consist of the
    // top-level TranslationUnitDecl. if this assert fires,
    // then it means ASTContext::setTraversalScope is being
    // (erroneously) used somewhere
    Assert(Context.getTraversalScope() ==
        std::vector<Decl*>{TU});

    for(auto* C : TU->decls())
        TraverseDecl(C);
}

void
ASTVisitor::
Initialize(ASTContext& Context)
{
    astContext_ = &Context;
    sourceManager_ = &Context.getSourceManager();
}

void
ASTVisitor::
InitializeSema(Sema& S)
{
    // Sema should not have been initialized yet
    Assert(! sema_);
    sema_ = &S;
}

void
ASTVisitor::
ForgetSema()
{
    sema_ = nullptr;
}

void
ASTVisitor::
HandleCXXStaticMemberVarInstantiation(VarDecl* D)
{
    // implicitly instantiated definitions of non-inline
    // static data members of class templates are added to
    // the end of the TU DeclContext. Decl::isImplicit returns
    // false for these VarDecls, so we manually set it here.
    D->setImplicit();
}

void
ASTVisitor::
HandleCXXImplicitFunctionInstantiation(FunctionDecl* D)
{
    D->setImplicit();
}

} // mrdox
} // clang
