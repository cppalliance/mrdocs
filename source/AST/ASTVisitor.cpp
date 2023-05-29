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
#include "ConfigImpl.hpp"
#include "Metadata/FunctionKind.hpp"
#include "Support/Path.hpp"
#include "Support/Debug.hpp"
#include <mrdox/Metadata.hpp>
#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclFriend.h>
#include <clang/Index/USRGeneration.h>
#include <clang/Lex/Lexer.h>
#include <llvm/ADT/Hashing.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/SHA1.h>
#include <cassert>
#include <ranges>

#include <clang/Frontend/CompilerInstance.h>
// #include <clang/Sema/Sema.h>
// #include <clang/Sema/Template.h>

namespace clang {
namespace mrdox {

namespace {

std::string getDeclName(const Decl* D)
{
    if(const auto* N = dyn_cast<NamedDecl>(D))
        return N->getQualifiedNameAsString();
    return "<unnamed>";
}

std::optional<std::size_t> getNumSpecializations(Decl* D)
{
    std::optional<std::size_t> r;
    if(auto* T = dyn_cast<ClassTemplateDecl>(D))
    {
        r.emplace();
        if(T->specializations().empty())
            return r;
        for([[maybe_unused]] auto&& S : T->specializations())
        {
#if 0
            print_debug("                ");
            DeclContext* DC = S->getDeclContext();
            do
            {
                Decl* ID = dyn_cast<Decl>(DC);
                print_debug("{} ({}) | ", getDeclName(ID), ID->getDeclKindName());
            }
            while(DC = DC->getParent());
            print_debug("\n");
#endif
            ++r.value();
        }
    }
    else if(auto* T = dyn_cast<FunctionTemplateDecl>(D))
    {
        r.emplace();
        for([[maybe_unused]] auto&& S : T->specializations())
            ++r.value();
    }
    else if(auto* T = dyn_cast<VarTemplateDecl>(D))
    {
        r.emplace();
        for([[maybe_unused]] auto&& S : T->specializations())
            ++r.value();
    }
    return r;
}

std::string indent(
    std::size_t depth,
    std::size_t indent_size = 4)
{
    return std::string(depth * indent_size, ' ');
}

const CXXRecordDecl*
getInstantiatedFromRecord(
    const ClassTemplatePartialSpecializationDecl* CTPSD)
{
    if(auto* MT = CTPSD->getInstantiatedFromMember())
        return MT;
    return CTPSD;
}

const CXXRecordDecl*
getInstantiatedFromRecord(
    const ClassTemplateSpecializationDecl* CTSD)
{
    auto parent = CTSD->getSpecializedTemplateOrPartial();
    // an explicitly specialized member of a partial specialization
    // is not a member of the primary template; treat it as such.
    if(auto* CTPSD = parent.dyn_cast<ClassTemplatePartialSpecializationDecl*>())
        return getInstantiatedFromRecord(CTPSD);

    ClassTemplateDecl* CTD = CTSD->getSpecializedTemplate();
    Assert(CTD);
    // for implicit specializations of a primary member template,
    // consider the parent context to be the primary template
    if(ClassTemplateDecl* MT = CTD->getInstantiatedFromMemberTemplate())
        return MT->getTemplatedDecl();
    return CTD->getTemplatedDecl();
}

} // anon

//------------------------------------------------
//
// ASTVisitor
//
//------------------------------------------------

ASTVisitor::
ASTVisitor(
    tooling::ExecutionContext& ex,
    ConfigImpl const& config,
    clang::CompilerInstance& compiler,
    Reporter& R) noexcept
    : ex_(ex)
    , config_(config)
    , R_(R)
    , PublicOnly(! config_.includePrivate)
    , IsFileInRootDir(true)
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
    SymbolID& id, const NamedDecl* D)
{
    usr_.clear();
    if(index::generateUSRForDecl(D, usr_))
        return false;
    id = SymbolID(llvm::SHA1::hash(
        arrayRefFromStringRef(usr_)).data());
    return true;
}

bool
ASTVisitor::
shouldSerializeInfo(
    bool PublicOnly,
    bool IsOrIsInAnonymousNamespace,
    const NamedDecl* D) noexcept
{
    // FIXME: this isn't right
    if(! PublicOnly)
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
}

//------------------------------------------------

/** Returns true if D is or is in an anonymous namespace
*/
bool
ASTVisitor::
getParentNamespaces(
    llvm::SmallVector<Reference, 4>& Namespaces,
    const Decl* D)
{
    bool anonymous = false;
    if(auto const* N = dyn_cast<NamespaceDecl>(D))
        anonymous = N->isAnonymousNamespace();

    const DeclContext* DC = D->getDeclContext();
    do
    {
        SymbolID id = SymbolID::zero;
        if(const auto* N = dyn_cast<NamespaceDecl>(DC))
        {
            std::string Namespace;
            if(N->isAnonymousNamespace())
            {
                Namespace = "@nonymous_namespace";
                anonymous = true;
            }
            else
            {
                Namespace = N->getNameAsString();
            }
            extractSymbolID(id, N);
            Namespaces.emplace_back(
                id,
                Namespace,
                InfoType::IT_namespace);
        }
#if 0
        // for an explicit specialization of member of an implicit specialization,
        // treat it as-if it was declared in the primary class template
        else if(const auto* N = dyn_cast<ClassTemplateSpecializationDecl>(DC); N &&
            N->getSpecializationKind() == TemplateSpecializationKind::TSK_ImplicitInstantiation)
        {
            // ClassTemplateDecl* P = N->getSpecializedTemplate();
            NamedDecl* TD = nullptr;
            auto parent = N->getSpecializedTemplateOrPartial();
            // an explicitly specialized member of a partial specialization
            // is not a member of the primary template; treat it as such.
            if(auto* partial = parent.dyn_cast<ClassTemplatePartialSpecializationDecl*>())
            {
                if(auto* MT = partial->getInstantiatedFromMember())
                    TD = MT;
                else
                    TD = partial;
            }
            else if(ClassTemplateDecl* PT = N->getSpecializedTemplate())
            {
                // for implicit specializations of a primary member template,
                // consider the parent context to be the primary template
                if(auto* MT = PT->getInstantiatedFromMemberTemplate())
                    TD = MT;
                else
                    TD = PT;
            }
            Assert(TD);

            extractSymbolID(id, TD);
            Namespaces.emplace_back(
                id,
                N->getNameAsString(),
                InfoType::IT_record);
        }
        else if(const auto* N = dyn_cast<RecordDecl>(DC))
        {
            extractSymbolID(id, N);
            Namespaces.emplace_back(
                id,
                N->getNameAsString(),
                InfoType::IT_record);
        }
#else
        else if(const auto* N = dyn_cast<CXXRecordDecl>(DC))
        {
            // if the containing context is an implicit specialization,
            // get the template from which it was instantiated
            if(const auto* S = dyn_cast<ClassTemplateSpecializationDecl>(DC);
                S && S->getSpecializationKind() == TSK_ImplicitInstantiation)
            {
                N = S->getTemplateInstantiationPattern();
            }
            extractSymbolID(id, N);
            Namespaces.emplace_back(
                id,
                N->getNameAsString(),
                InfoType::IT_record);
        }
#endif
        else if(const auto* N = dyn_cast<FunctionDecl>(DC))
        {
            extractSymbolID(id, N);
            Namespaces.emplace_back(
                id,
                N->getNameAsString(),
                InfoType::IT_function);
        }
        else if(const auto* N = dyn_cast<EnumDecl>(DC))
        {
            extractSymbolID(id, N);
            Namespaces.emplace_back(
                id,
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
            SymbolID::zero,
            "", //"GlobalNamespace",
            InfoType::IT_namespace);
    }

    return anonymous;
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
        InfoType IT;
        if(isa<EnumDecl>(TD))
            IT = InfoType::IT_enum;
        else if(isa<RecordDecl>(TD))
            IT = InfoType::IT_record;
        else
            IT = InfoType::IT_default;
        extractSymbolID(id, TD);
        return TypeInfo(Reference(
            id, TD->getNameAsString(), IT));
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

template<typename R>
void
ASTVisitor::
buildTemplateArgs(
    TemplateInfo& I,
    R&& args)
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
        extractSymbolID(I.Primary.emplace(), primary);
    }
    // KRYSTIAN NOTE: when this is a partial specialization, we could use
    // ClassTemplatePartialSpecializationDecl::getTemplateArgsAsWritten
    const TypeSourceInfo* tsi = spec->getTypeAsWritten();
    // type source information *should* be non-null
    Assert(tsi);
    auto args = tsi->getType()->getAs<
        TemplateSpecializationType>()->template_arguments();
    buildTemplateArgs(I, args);
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
        extractSymbolID(I.Primary.emplace(), primary);
    }

    // spec->getTemplateArgsInfo()
    if(auto* partial = dyn_cast<VarTemplatePartialSpecializationDecl>(spec);
        partial && partial->getTemplateArgsAsWritten())
    {
        auto args = partial->getTemplateArgsAsWritten()->arguments();
        buildTemplateArgs(I, std::views::transform(
            args, [](auto& x) -> auto& { return x.getArgument(); }));
    }
    else
    {
        buildTemplateArgs(I, spec->getTemplateArgs().asArray());
    }
    // buildTemplateArgs(I, spec->getTemplateInstantiationArgs().asArray());
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
        extractSymbolID(I.Primary.emplace(), primary);
    }
    if(auto* args = spec->TemplateArguments)
    {
        // spec->TemplateArgumentsAsWritten->arguments();
        buildTemplateArgs(I, args->asArray());
    }
}

void
ASTVisitor::
parseTemplateArgs(
    TemplateInfo& I,
    const ClassScopeFunctionSpecializationDecl* spec)
{
    // if(! spec->hasExplicitTemplateArgs())
    //     return;
    // KRYSTIAN NOTE: we have no way to get the ID of the primary template.
    // in the future, we could use name lookup to find matching declarations
    if(auto* args_written = spec->getTemplateArgsAsWritten())
    {
        auto args = args_written->arguments();
        buildTemplateArgs(I, std::views::transform(
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
    std::optional<Javadoc>& javadoc,
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

template<class Member>
static
void
insertChild(
    RecordInfo& P, Member const& I, Access access)
{
    if constexpr(std::is_same_v<Member, RecordInfo>)
    {
        P.Members.Records.push_back({ I.id, access });
    }
    else if constexpr(std::is_same_v<Member, FunctionInfo>)
    {
        P.Members.Functions.push_back({ I.id, access });
    }
    else if constexpr(std::is_same_v<Member, TypedefInfo>)
    {
        P.Members.Types.push_back({ I.id, access });
    }
    else if constexpr(std::is_same_v<Member, EnumInfo>)
    {
        P.Members.Enums.push_back({ I.id, access });
    }
    else if constexpr(std::is_same_v<Member, FieldInfo>)
    {
        P.Members.Fields.push_back({ I.id, access });
    }
    else if constexpr(std::is_same_v<Member, VarInfo>)
    {
        P.Members.Vars.push_back({ I.id, access });
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
    // KRYSTIAN NOTE: Child should *never* be FieldInfo
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
            if(I.id == SymbolID::zero)
            {
                // Global namespace has no parent.
                return {};
            }

            // In global namespace
            NamespaceInfo P;
            Assert(P.id == SymbolID::zero);
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
    // and insert the child as a MemberRef.
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

    // skip system header
    if(sourceManager_->isInSystemHeader(D->getLocation()))
        return false;

    // we should never visit block scope declarations
    Assert(! D->getParentFunctionOrMethod());

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
extractInfo(
    Info& I,
    NamedDecl const* D)
{
    bool anonymous = getParentNamespaces(I.Namespace, D);
    if(! shouldSerializeInfo(PublicOnly, anonymous, D))
        return false;
    if(! extractSymbolID(I.id, D))
        return false;
    I.Name = D->getNameAsString();
    parseRawComment(I.javadoc, D, R_);
    return true;
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

        SymbolID id = SymbolID::zero;
        if(auto const* Ty = B.getType()->getAs<TemplateSpecializationType>())
        {
            TemplateDecl const* TD = Ty->getTemplateName().getAsTemplateDecl();
            extractSymbolID(id, TD);
            I.Bases.emplace_back(
                id,
                getTypeAsString(B.getType()),
                getAccessFromSpecifier(B.getAccessSpecifier()),
                isVirtual);
        }
        else if(RecordDecl const* P = getCXXRecordDeclForType(B.getType()))
        {
            extractSymbolID(id, P);
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

template<class DeclTy>
bool
ASTVisitor::
constructFunction(
    FunctionInfo& I,
    DeclTy* D)
{
    // adjust parameter types
    applyDecayToParameters(D);
    if(! extractInfo(I, D))
        return false;
    // if(name)
    //     I.Name = name;
    LineNumber = getLine(D);
    if(D->isThisDeclarationADefinition())
        I.DefLoc.emplace(LineNumber, File, IsFileInRootDir);
    else
        I.Loc.emplace_back(LineNumber, File, IsFileInRootDir);
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
    insertBitcode(ex_, writeBitcode(I));
    insertBitcode(ex_, writeParent(I));
}

void
ASTVisitor::
buildRecord(
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
#if 0
            SymbolID id;
            getParent(id, D);
#else
            const DeclContext* DC = D->getDeclContext();
            const auto* RD = dyn_cast<RecordDecl>(DC);
            Assert(RD && "the semantic DeclContext of a FriendDecl must be a class");
            RecordInfo P;
            extractSymbolID(P.id, RD);

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
    EnumDecl* D)
{
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
buildField(
    FieldInfo& I,
    FieldDecl* D)
{
    if(! extractInfo(I, D))
        return;
    LineNumber = getLine(D);
    I.DefLoc.emplace(LineNumber, File, IsFileInRootDir);

    I.Type = getTypeInfoForType(
        D->getTypeSourceInfo()->getType());

    I.specs.hasNoUniqueAddress = D->hasAttr<NoUniqueAddressAttr>();
    I.specs.isDeprecated = D->hasAttr<DeprecatedAttr>();
    // KRYSTIAN FIXME: isNodiscard should be isMaybeUnused
    I.specs.isNodiscard = D->hasAttr<UnusedAttr>();

    insertBitcode(ex_, writeBitcode(I));
    insertBitcode(ex_, writeParent(I, D->getAccess()));
}

void
ASTVisitor::
buildVar(
    VarInfo& I,
    VarDecl* D)
{
    if(! extractInfo(I, D))
        return;
    LineNumber = getLine(D);
    if(D->isThisDeclarationADefinition())
        I.DefLoc.emplace(LineNumber, File, IsFileInRootDir);
    else
        I.Loc.emplace_back(LineNumber, File, IsFileInRootDir);
    I.Type = getTypeInfoForType(
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
    DeclTy* D)
{
    if(! constructFunction(I, D))
        return;

    insertBitcode(ex_, writeBitcode(I));
    insertBitcode(ex_, writeParent(I,
        std::derived_from<DeclTy, CXXMethodDecl> ?
        D->getAccess() : AccessSpecifier::AS_none));
}

template<class DeclTy>
void
ASTVisitor::
buildTypedef(
    TypedefInfo& I,
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

    LineNumber = getLine(D);
    // D->isThisDeclarationADefinition(); // not available
    I.DefLoc.emplace(LineNumber, File, IsFileInRootDir);
    // KRYSTIAN NOTE: IsUsing is set by TraverseTypeAlias
    // I.IsUsing = std::is_same_v<DeclTy, TypeAliasDecl>;
    insertBitcode(ex_, writeBitcode(I));
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

    NamespaceInfo I;
    buildNamespace(I, D);

    return TraverseContext(D);
}

bool
ASTVisitor::
Traverse(
    CXXRecordDecl* D,
    std::unique_ptr<TemplateInfo>&& Template)
{
    if(! shouldExtract(D))
        return true;

    RecordInfo I(std::move(Template));
    buildRecord(I, D);

    return TraverseContext(D);
}

bool
ASTVisitor::
Traverse(
    TypedefDecl* D)
{
    if(! shouldExtract(D))
        return true;

    TypedefInfo I;
    buildTypedef(I, D);
    return true;
}

bool
ASTVisitor::
Traverse(
    TypeAliasDecl* D,
    std::unique_ptr<TemplateInfo>&& Template)
{
    if(! shouldExtract(D))
        return true;

    TypedefInfo I(std::move(Template));
    I.IsUsing = true;

    buildTypedef(I, D);
    return true;
}

bool
ASTVisitor::
Traverse(
    VarDecl* D,
    std::unique_ptr<TemplateInfo>&& Template)
{
    if(! shouldExtract(D))
        return true;

    VarInfo I(std::move(Template));
    buildVar(I, D);
    return true;
}

bool
ASTVisitor::
Traverse(
    FunctionDecl* D,
    std::unique_ptr<TemplateInfo>&& Template)
{
    if(! shouldExtract(D))
        return true;
    FunctionInfo I(std::move(Template));
    buildFunction(I, D);
    return true;
}

bool
ASTVisitor::
Traverse(
    CXXMethodDecl* D,
    std::unique_ptr<TemplateInfo>&& Template)
{
    if(! shouldExtract(D))
        return true;
    FunctionInfo I(std::move(Template));
    buildFunction(I, D);
    return true;
}

bool
ASTVisitor::
Traverse(
    CXXConstructorDecl* D,
    std::unique_ptr<TemplateInfo>&& Template)
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
    FunctionInfo I(std::move(Template));
    buildFunction(I, D);
    return true;
}

bool
ASTVisitor::
Traverse(
    CXXConversionDecl* D,
    std::unique_ptr<TemplateInfo>&& Template)
{
    if(! shouldExtract(D))
        return true;
    FunctionInfo I(std::move(Template));
    buildFunction(I, D);
    return true;
}

bool
ASTVisitor::
Traverse(
    CXXDeductionGuideDecl* D,
    std::unique_ptr<TemplateInfo>&& Template)
{
    if(! shouldExtract(D))
        return true;
    FunctionInfo I(std::move(Template));
    buildFunction(I, D);
    return true;
}

bool
ASTVisitor::
Traverse(
    CXXDestructorDecl* D)
{
    if(! shouldExtract(D))
        return true;
    FunctionInfo I;
    buildFunction(I, D);
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
    EnumDecl* D)
{
    if(! shouldExtract(D))
        return true;

    EnumInfo I;
    buildEnum(I, D);
    return true;
}

bool
ASTVisitor::
Traverse(
    FieldDecl* D)
{
    if(! shouldExtract(D))
        return true;
    FieldInfo I;
    buildField(I, D);
    return true;
}

bool
ASTVisitor::
Traverse(
    ClassTemplateDecl* D)
{
    CXXRecordDecl* RD = D->getTemplatedDecl();
    if(! shouldExtract(RD))
        return true;

    auto Template = std::make_unique<TemplateInfo>();
    parseTemplateParams(*Template, RD);

    return Traverse(RD, std::move(Template));
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

    return Traverse(RD, std::move(Template));
}

bool
ASTVisitor::
Traverse(
    ClassTemplatePartialSpecializationDecl* D)
{
    // without this function, we would only traverse
    // explicit specialization declarations
    return Traverse(static_cast<
        ClassTemplateSpecializationDecl*>(D));
}

bool
ASTVisitor::
Traverse(
    VarTemplateDecl* D)
{
    VarDecl* VD = D->getTemplatedDecl();
    if(! shouldExtract(VD))
        return true;

    auto Template = std::make_unique<TemplateInfo>();
    parseTemplateParams(*Template, VD);

    return Traverse(VD, std::move(Template));
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

    return Traverse(VD, std::move(Template));
}

bool
ASTVisitor::
Traverse(
    VarTemplatePartialSpecializationDecl* D)
{
    return Traverse(static_cast<
        VarTemplateSpecializationDecl*>(D));
}

bool
ASTVisitor::
Traverse(
    FunctionTemplateDecl* D)
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

    // for class scope explicit specializations of member function templates which
    // are members of class templates, it is impossible to know what the
    // primary template is until the enclosing class template is instantiated.
    // while such declarations are valid C++ (see CWG 727 and [temp.expl.spec] p3),
    // GCC does not consider them to be valid. consequently, we do not extract the SymbolID
    // of the primary template. in the future, we could take a best-effort approach to find
    // the primary template, but this is only possible when none of the candidates are dependent
    // upon a template parameter of the enclosing class template.
    auto Template = std::make_unique<TemplateInfo>();
    parseTemplateArgs(*Template, D);

    CXXMethodDecl* MD = D->getSpecialization();

    // KRYSTIAN FIXME: is the right? should this call TraverseDecl instead?
    return Traverse(MD, std::move(Template));
}

bool
ASTVisitor::
Traverse(
    TypeAliasTemplateDecl* D)
{
    TypeAliasDecl* AD = D->getTemplatedDecl();
    if(! shouldExtract(AD))
        return true;

    auto Template = std::make_unique<TemplateInfo>();
    parseTemplateParams(*Template, AD);

    return Traverse(AD, std::move(Template));
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

#if 1
    if(D->isImplicit())
        return true;
#else
    {
        auto specs = getNumSpecializations(D);
        std::string specs_str = specs.has_value() ?
            std::format("specializations = {}", *specs) : "";
        if(D->isImplicit())
        {
            print_debug("{:<64}{:<32}{}\n", std::format(
                "{}(implicit) {}", indent(context_depth_), D->getDeclKindName()),
                getDeclName(D), specs_str);
            if(auto* DC = dyn_cast<DeclContext>(D))
                TraverseContext(DC);
            return true;
        }
        print_debug("{:<64}{:<32}{}\n", std::format(
            "{}{}", indent(context_depth_), D->getDeclKindName()),
            getDeclName(D), specs_str);
    }
#endif

    switch(D->getKind())
    {
    case Decl::Namespace:
        this->Traverse(static_cast<
            NamespaceDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::CXXRecord:
        this->Traverse(static_cast<
            CXXRecordDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::CXXMethod:
        this->Traverse(static_cast<
            CXXMethodDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::CXXConstructor:
        this->Traverse(static_cast<
            CXXConstructorDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::CXXConversion:
        this->Traverse(static_cast<
            CXXConversionDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::CXXDestructor:
        this->Traverse(static_cast<
            CXXDestructorDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::CXXDeductionGuide:
        this->Traverse(static_cast<
            CXXDeductionGuideDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::Function:
        this->Traverse(static_cast<
            FunctionDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::Friend:
        this->Traverse(static_cast<
            FriendDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::TypeAlias:
        this->Traverse(static_cast<
            TypeAliasDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::Typedef:
        this->Traverse(static_cast<
            TypedefDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::Enum:
        this->Traverse(static_cast<
            EnumDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::Field:
        this->Traverse(static_cast<
            FieldDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::Var:
        this->Traverse(static_cast<
            VarDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::ClassTemplate:
        this->Traverse(static_cast<
            ClassTemplateDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::ClassTemplateSpecialization:
        this->Traverse(static_cast<
            ClassTemplateSpecializationDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::ClassTemplatePartialSpecialization:
        this->Traverse(static_cast<
            ClassTemplatePartialSpecializationDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::VarTemplate:
        this->Traverse(static_cast<
            VarTemplateDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::VarTemplateSpecialization:
        this->Traverse(static_cast<
            VarTemplateSpecializationDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::VarTemplatePartialSpecialization:
        this->Traverse(static_cast<
            VarTemplatePartialSpecializationDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::FunctionTemplate:
        this->Traverse(static_cast<
            FunctionTemplateDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::ClassScopeFunctionSpecialization:
        this->Traverse(static_cast<
            ClassScopeFunctionSpecializationDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::TypeAliasTemplate:
        this->Traverse(static_cast<
            TypeAliasTemplateDecl*>(D),
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
    ++context_depth_;
    for(auto* C : D->decls())
        TraverseDecl(C);
    --context_depth_;
    return true;
}

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

    std::optional<llvm::StringRef> filePath =
        Context.getSourceManager().getNonBuiltinFilenameForID(
            Context.getSourceManager().getMainFileID());
    if(! filePath)
        return;

    // Filter out TUs we don't care about
    File = *filePath;
    convert_to_slash(File);
    if(! config_.shouldVisitTU(File))
        return;

    context_depth_ = 0;
    sema_ = &compiler_.getSema();

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

} // mrdox
} // clang
