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

#include <mrdocs/Platform.hpp>
#include <lib/AST/ASTVisitor.hpp>
#include <lib/AST/ClangHelpers.hpp>
#include <lib/AST/ExtractDocComment.hpp>
#include <lib/AST/NameBuilder.hpp>
#include <lib/AST/TypeBuilder.hpp>
#include <lib/Diagnostics.hpp>
#include <lib/Support/Path.hpp>
#include <lib/Support/Radix.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Support/Algorithm.hpp>
#include <mrdocs/Support/ScopeExit.hpp>
#include <clang/AST/AST.h>
#include <clang/AST/Attr.h>
#include <clang/AST/ODRHash.h>
#include <clang/AST/TypeVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Index/USRGeneration.h>
#include <clang/Lex/Lexer.h>
#include <clang/Sema/Lookup.h>
#include <clang/Sema/Sema.h>
#include <clang/Sema/Template.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/Process.h>
#include <llvm/Support/SHA1.h>
#include <memory>
#include <optional>
#include <ranges>
#include <unordered_set>

namespace mrdocs {

ASTVisitor::
ASTVisitor(
    ConfigImpl const& config,
    Diagnostics const& diags,
    clang::CompilerInstance& compiler,
    clang::ASTContext& context,
    clang::Sema& sema) noexcept
    : config_(config)
    , diags_(diags)
    , compiler_(compiler)
    , context_(context)
    , source_(context.getSourceManager())
    , sema_(sema)
{
    // Install handlers for our custom commands
    initCustomCommentCommands(context_);

    // The traversal scope should *only* consist of the
    // top-level clang::TranslationUnitDecl.
    // If this `assert` fires, then it means
    // ASTContext::setTraversalScope is being (erroneously)
    // used somewhere
    MRDOCS_ASSERT(context_.getTraversalScope() ==
                  llvm::ArrayRef<clang::Decl *>{context_.getTranslationUnitDecl()});
}

void
ASTVisitor::
build()
{
    // Traverse the translation unit, only extracting
    // declarations which satisfy all filter conditions.
    // dependencies will be tracked, but not extracted
    clang::TranslationUnitDecl const* TU = context_.getTranslationUnitDecl();
    traverse(TU);
    MRDOCS_ASSERT(find(SymbolID::global));
}

template <
    class InfoTy,
    std::derived_from<clang::Decl> DeclTy>
Symbol*
ASTVisitor::
traverse(DeclTy const* D)
{
    MRDOCS_ASSERT(D);
    MRDOCS_CHECK_OR(!D->isInvalidDecl(), nullptr);
    MRDOCS_SYMBOL_TRACE(D, context_);

    if constexpr (std::same_as<DeclTy, clang::Decl>)
    {
        // Convert to the most derived type of the Decl
        // and call the appropriate traverse function
        return visit(D, [&]<typename DeclTyU>(DeclTyU* U) -> Symbol*
        {
            if constexpr (!std::same_as<DeclTyU, clang::Decl>)
            {
                return traverse(U);
            }
            return nullptr;
        });
    }
    else if constexpr (HasInfoTypeFor<DeclTy> || std::derived_from<InfoTy, Symbol>)
    {
        // If the declaration has a corresponding Info type,
        // we build the Info object and populate it with the
        // necessary information.
        using R = std::conditional_t<
            std::same_as<InfoTy, void>,
            InfoTypeFor_t<DeclTy>,
            InfoTy>;

        auto exp = upsert<R>(D);
        MRDOCS_CHECK_OR(exp, nullptr);
        auto& I = exp->I;
        auto& isNew = exp->isNew;

        // Populate the base classes with the necessary information.
        // Even when the object is new, we want to update the source locations
        // and the documentation status.
        populate(I.asInfo(), isNew, D);

        // Populate the derived Info object with the necessary information
        // when the object is new. If the object already exists, this
        // information would be redundant.
        populate(I, D);

        // Traverse the members of the declaration according to the
        // current extraction mode.
        traverseMembers(I, D);

        // Traverse the parents of the declaration in dependency mode.
        traverseParent(I, D);

        return &I;
    }
    return nullptr;
}

Symbol*
ASTVisitor::
traverse(clang::FunctionTemplateDecl const* D)
{
    // Route the traversal to GuideSymbol or FunctionSymbol
    if (clang::FunctionDecl* FD = D->getTemplatedDecl();
        FD && isa<clang::CXXDeductionGuideDecl>(FD))
    {
        return traverse<GuideSymbol>(D);
    }
    return traverse<FunctionSymbol>(D);
}

Symbol*
ASTVisitor::
traverse(clang::UsingDirectiveDecl const* D)
{
    MRDOCS_SYMBOL_TRACE(D, context_);

    // Find the parent namespace
    ScopeExitRestore s1(mode_, TraversalMode::Dependency);
    clang::Decl const* P = getParent(D);
    MRDOCS_SYMBOL_TRACE(P, context_);
    Symbol* PI = findOrTraverse(P);
    MRDOCS_CHECK_OR(PI, nullptr);
    MRDOCS_CHECK_OR(PI->isNamespace(), nullptr);
    auto& PNI = PI->asNamespace();

    // Find the nominated namespace
    clang::Decl const* ND = D->getNominatedNamespace();
    MRDOCS_SYMBOL_TRACE(ND, context_);
    ScopeExitRestore s2(mode_, TraversalMode::Dependency);
    Symbol* NDI = findOrTraverse(ND);
    MRDOCS_CHECK_OR(NDI, nullptr);

    Optional<Polymorphic<Name>> res = toName(ND);
    MRDOCS_CHECK_OR(res, nullptr);
    MRDOCS_ASSERT(!res->valueless_after_move());
    MRDOCS_ASSERT((*res)->isIdentifier());
    if (Name NI = **res;
        !contains(PNI.UsingDirectives, NI))
    {
        PNI.UsingDirectives.push_back(std::move(NI));
    }
    return nullptr;
}

Symbol*
ASTVisitor::
traverse(clang::IndirectFieldDecl const* D)
{
    return traverse(D->getAnonField());
}

template <
    std::derived_from<Symbol> InfoTy,
    std::derived_from<clang::Decl> DeclTy>
requires (!std::derived_from<DeclTy, clang::RedeclarableTemplateDecl>)
void
ASTVisitor::
traverseMembers(InfoTy& I, DeclTy const* DC)
{
    // When a declaration context is a function, we should
    // not traverse its members as function arguments are
    // not main Info members.
    if constexpr (
        !std::derived_from<DeclTy, clang::FunctionDecl> &&
        std::derived_from<DeclTy, clang::DeclContext>)
    {
        // We only need members of regular symbols and see-below namespaces
        // - If symbol is SeeBelow we want the members if it's a namespace
        MRDOCS_CHECK_OR(
            I.Extraction != ExtractionMode::SeeBelow ||
            I.Kind == SymbolKind::Namespace);

        // - If symbol is a Dependency, we only want the members if
        //   the traversal mode is BaseClass
        MRDOCS_CHECK_OR(
            I.Extraction != ExtractionMode::Dependency ||
            mode_ == TraversalMode::BaseClass);

        // - If symbol is ImplementationDefined, we only want the members if
        //   the traversal mode is BaseClass
        MRDOCS_CHECK_OR(
            I.Extraction != ExtractionMode::ImplementationDefined ||
            mode_ == TraversalMode::BaseClass);

        // There are many implicit declarations, especially in the
        // translation unit declaration, so we preemtively skip them here.
        auto explicitMembers = std::ranges::views::filter(DC->decls(), [](clang::Decl* D)
            {
                return !D->isImplicit() || isa<clang::IndirectFieldDecl>(D);
            });
        for (auto* D : explicitMembers)
        {
            // No matter what happens in the process, we restore the
            // traversal mode to the original mode for the next member
            ScopeExitRestore s(mode_);
            // Traverse the member
            traverse(D);
        }
    }
}

template <
    std::derived_from<Symbol> InfoTy,
    std::derived_from<clang::RedeclarableTemplateDecl> DeclTy>
void
ASTVisitor::
traverseMembers(InfoTy& I, DeclTy const* D)
{
    traverseMembers(I, D->getTemplatedDecl());
}

template <
    std::derived_from<Symbol> InfoTy,
    std::derived_from<clang::Decl> DeclTy>
requires (!std::derived_from<DeclTy, clang::RedeclarableTemplateDecl>)
void
ASTVisitor::
traverseParent(InfoTy& I, DeclTy const* DC)
{
    MRDOCS_SYMBOL_TRACE(DC, context_);
    if (clang::Decl const* PD = getParent(DC))
    {
        MRDOCS_SYMBOL_TRACE(PD, context_);

        // Check if we haven't already extracted or started
        // to extract the parent scope:
        // Traverse the parent scope as a dependency if it
        // hasn't been extracted yet
        Symbol* PI = nullptr;
        {
            ScopeExitRestore s(mode_, Dependency);
            if (PI = findOrTraverse(PD); !PI)
            {
                return;
            }
        }

        // If we found the parent scope, set it as the parent
        I.Parent = PI->id;

        visit(*PI, [&]<typename ParentInfoTy>(ParentInfoTy& PU) -> void
        {
            if constexpr (SymbolParent<ParentInfoTy>)
            {
                addMember(PU, I);
            }
        });
    }
}

template <
    std::derived_from<Symbol> InfoTy,
    std::derived_from<clang::RedeclarableTemplateDecl> DeclTy>
void
ASTVisitor::
traverseParent(InfoTy& I, DeclTy const* D)
{
    traverseParent(I, D->getTemplatedDecl());
}


Expected<llvm::SmallString<128>>
ASTVisitor::
generateUSR(clang::Decl const* D) const
{
    MRDOCS_ASSERT(D);
    llvm::SmallString<128> res;

    if (auto const* NAD = dyn_cast<clang::NamespaceAliasDecl>(D))
    {
        if (clang::index::generateUSRForDecl(cast<clang::Decl>(NAD->getNamespace()), res))
        {
            return Unexpected(Error("Failed to generate USR"));
        }
        res.append("@NA");
        res.append(NAD->getNameAsString());
        return res;
    }

    // Handling UsingDecl
    if (auto const* UD = dyn_cast<clang::UsingDecl>(D))
    {
        for (auto const* shadow : UD->shadows())
        {
            if (clang::index::generateUSRForDecl(shadow->getTargetDecl(), res))
            {
                return Unexpected(Error("Failed to generate USR"));
            }
        }
        res.append("@UDec");
        res.append(UD->getQualifiedNameAsString());
        return res;
    }

    if (auto const* UD = dyn_cast<clang::UsingDirectiveDecl>(D))
    {
        if (clang::index::generateUSRForDecl(UD->getNominatedNamespace(), res))
        {
            return Unexpected(Error("Failed to generate USR"));
        }
        res.append("@UDDec");
        res.append(UD->getQualifiedNameAsString());
        return res;
    }

    // Handling clang::UnresolvedUsingTypenameDecl
    if (auto const* UD = dyn_cast<clang::UnresolvedUsingTypenameDecl>(D))
    {
        if (clang::index::generateUSRForDecl(UD, res))
        {
            return Unexpected(Error("Failed to generate USR"));
        }
        res.append("@UUTDec");
        res.append(UD->getQualifiedNameAsString());
        return res;
    }

    // Handling clang::UnresolvedUsingValueDecl
    if (auto const* UD = dyn_cast<clang::UnresolvedUsingValueDecl>(D))
    {
        if (clang::index::generateUSRForDecl(UD, res))
        {
            return Unexpected(Error("Failed to generate USR"));
        }
        res.append("@UUV");
        res.append(UD->getQualifiedNameAsString());
        return res;
    }

    // Handling clang::UsingPackDecl
    if (auto const* UD = dyn_cast<clang::UsingPackDecl>(D))
    {
        if (clang::index::generateUSRForDecl(UD, res))
        {
            return Unexpected(Error("Failed to generate USR"));
        }
        res.append("@UPD");
        res.append(UD->getQualifiedNameAsString());
        return res;
    }

    // Handling clang::UsingEnumDecl
    if (auto const* UD = dyn_cast<clang::UsingEnumDecl>(D))
    {
        if (clang::index::generateUSRForDecl(UD, res))
        {
            return Unexpected(Error("Failed to generate USR"));
        }
        res.append("@UED");
        clang::EnumDecl const* ED = UD->getEnumDecl();
        res.append(ED->getQualifiedNameAsString());
        return res;
    }

    // KRYSTIAN NOTE: clang doesn't currently support
    // generating USRs for friend declarations, so we
    // will improvise until I can merge a patch which
    // adds support for them
    if(auto const* FD = dyn_cast<clang::FriendDecl>(D))
    {
        // first, generate the USR for the containing class
        if (clang::index::generateUSRForDecl(cast<clang::Decl>(FD->getDeclContext()), res))
        {
            return Unexpected(Error("Failed to generate USR"));
        }
        // add a seperator for uniqueness
        res.append("@FD");
        // if the friend declaration names a type,
        // use the USR generator for types
        if (clang::TypeSourceInfo* TSI = FD->getFriendType())
        {
            if (clang::index::generateUSRForType(TSI->getType(), context_, res))
            {
                return Unexpected(Error("Failed to generate USR"));
            }
            return res;
        }
        // otherwise, fallthrough and append the
        // USR of the nominated declaration
        if (!((D = FD->getFriendDecl())))
        {
            return Unexpected(Error("Failed to generate USR"));
        }
    }

    if (clang::index::generateUSRForDecl(D, res))
    {
        return Unexpected(Error("Failed to generate USR"));
    }

    auto const* Described = dyn_cast_if_present<clang::TemplateDecl>(D);
    auto const* Templated = D;
    if (auto const* DT = D->getDescribedTemplate())
    {
        Described = DT;
        if (auto const* TD = DT->getTemplatedDecl())
        {
            Templated = TD;
        }
    }

    if(Described)
    {
        clang::TemplateParameterList const* TPL = Described->getTemplateParameters();
        if(auto const* RC = TPL->getRequiresClause())
        {
            RC = SubstituteConstraintExpressionWithoutSatisfaction(
                sema_, cast<clang::NamedDecl>(isa<clang::FunctionTemplateDecl>(Described) ? Described : Templated), RC);
            if (!RC)
            {
                return Unexpected(Error("Failed to generate USR"));
            }
            clang::ODRHash odr_hash;
            odr_hash.AddStmt(RC);
            res.append("@TPL#");
            res.append(llvm::itostr(odr_hash.CalculateHash()));
        }
    }

    if(auto* FD = dyn_cast<clang::FunctionDecl>(Templated);
        FD && FD->getTrailingRequiresClause())
    {
        clang::Expr const* RC = FD->getTrailingRequiresClause().ConstraintExpr;
        RC = SubstituteConstraintExpressionWithoutSatisfaction(
            sema_, cast<clang::NamedDecl>(Described ? Described : Templated), RC);
        if (!RC)
        {
            return Unexpected(Error("Failed to generate USR"));
        }
        clang::ODRHash odr_hash;
        odr_hash.AddStmt(RC);
        res.append("@TRC#");
        res.append(llvm::itostr(odr_hash.CalculateHash()));
    }

    return res;
}

bool
ASTVisitor::
generateID(
    clang::Decl const* D,
    SymbolID& id) const
{
    if (!D)
    {
        return false;
    }

    if (isa<clang::TranslationUnitDecl>(D))
    {
        id = SymbolID::global;
        return true;
    }

    if (auto exp = generateUSR(D))
    {
        auto h = llvm::SHA1::hash(arrayRefFromStringRef(*exp));
        id = SymbolID(h.data());
        return true;
    }

    return false;
}

SymbolID
ASTVisitor::
generateID(clang::Decl const* D) const
{
    SymbolID id = SymbolID::invalid;
    generateID(D, id);
    return id;
}

template <std::derived_from<clang::Decl> DeclTy>
void
ASTVisitor::
populate(Symbol& I, bool const isNew, DeclTy const* D)
{
    populate(I.doc, D);
    populate(I.Loc, D);

    // All other information is redundant if the symbol is not new
    MRDOCS_CHECK_OR(isNew);

    // These should already have been populated by traverseImpl
    MRDOCS_ASSERT(I.id);
    MRDOCS_ASSERT(I.Kind != SymbolKind::None);

    I.Name = extractName(D);
}

template <std::derived_from<clang::Decl> DeclTy>
void
ASTVisitor::
populate(SourceInfo& I, DeclTy const* D)
{
    clang::SourceLocation Loc = D->getBeginLoc();
    if (Loc.isInvalid())
    {
        Loc = D->getLocation();
    }
    if (Loc.isValid())
    {
        populate(
            I,
            Loc,
            isDefinition(D),
            isDocumented(D));
    }
}

bool
ASTVisitor::
populate(
    Optional<DocComment>& doc,
    clang::Decl const* D)
{
    clang::RawComment const* RC = getDocumentation(D);
    MRDOCS_CHECK_OR(RC, false);
    clang::comments::FullComment* FC =
        RC->parse(D->getASTContext(), &sema_.getPreprocessor(), D);
    MRDOCS_CHECK_OR(FC, false);
    populateDocComment(doc, FC, D, config_, diags_);
    return true;
}

void
ASTVisitor::
populate(
    SourceInfo& I,
    clang::SourceLocation const loc,
    bool const definition,
    bool const documented)
{
    auto presLoc = source_.getPresumedLoc(loc, false);
    unsigned line = presLoc.getLine();
    unsigned col = presLoc.getColumn();
    FileInfo* file = findFileInfo(loc);

    // No file is not an error, it just typically means it has been generated
    // in the virtual filesystem.
    MRDOCS_CHECK_OR(file);

    Location Loc(file->full_path,
        file->short_path,
        file->source_path,
        line,
        col,
        documented);
    if (definition)
    {
        if (I.DefLoc)
        {
            return;
        }
        I.DefLoc = std::move(Loc);
    }
    else
    {
        auto const existing = std::ranges::
            find_if(I.Loc,
            [line, file](Location const& l)
            {
                return l.LineNumber == line &&
                    l.FullPath == file->full_path;
            });
        if (existing != I.Loc.end())
        {
            return;
        }
        I.Loc.push_back(std::move(Loc));
    }
}

void
ASTVisitor::
populate(
    NamespaceSymbol& I,
    clang::NamespaceDecl const* D)
{
    I.IsAnonymous = D->isAnonymousNamespace();
    if (!I.IsAnonymous)
    {
        I.Name = extractName(D);
    }
    I.IsInline = D->isInline();
}

void
ASTVisitor::
populate(
    NamespaceSymbol& I,
    clang::TranslationUnitDecl const*)
{
    I.id = SymbolID::global;
    I.IsAnonymous = false;
    I.IsInline = false;
}

void
ASTVisitor::
populate(
    RecordSymbol& I,
    clang::CXXRecordDecl const* D)
{
    if (D->getTypedefNameForAnonDecl())
    {
        I.IsTypeDef = true;
    }
    I.KeyKind = toRecordKeyKind(D->getTagKind());
    // These are from clang::CXXRecordDecl::isEffectivelyFinal()
    I.IsFinal = D->hasAttr<clang::FinalAttr>();
    if (auto const* DT = D->getDestructor())
    {
        I.IsFinalDestructor = DT->hasAttr<clang::FinalAttr>();
    }

    // Extract direct bases. D->bases() will get the bases
    // from whichever declaration is the definition (if any)
    if(D->hasDefinition() && I.Bases.empty())
    {
        for (clang::CXXBaseSpecifier const& B : D->bases())
        {
            clang::AccessSpecifier const access = B.getAccessSpecifier();

            if (!config_->extractPrivateBases &&
                access == clang::AS_private)
            {
                continue;
            }

            clang::QualType const BT = B.getType();
            auto BaseType = toType(BT, BaseClass);

            // If we're going to copy the members from the specialization,
            // we need to instantiate and traverse the specialization
            // as a dependency.
            if (config_->extractImplicitSpecializations)
            {
                [&] {
                    auto* TST = BT->getAs<clang::TemplateSpecializationType>();
                    MRDOCS_CHECK_OR(TST);
                    MRDOCS_SYMBOL_TRACE(TST, context_);

                    auto const* CTSD = dyn_cast_or_null<
                            clang::ClassTemplateSpecializationDecl>(
                            TST->getAsCXXRecordDecl());
                    MRDOCS_CHECK_OR(CTSD);
                    MRDOCS_SYMBOL_TRACE(CTSD, context_);

                    // Traverse the Decl as a dependency
                    ScopeExitRestore s(mode_, TraversalMode::BaseClass);
                    Symbol const* SI = findOrTraverse(CTSD);
                    MRDOCS_CHECK_OR(SI);
                    auto& inner = innermostType(BaseType);
                    MRDOCS_CHECK_OR(inner);
                    MRDOCS_CHECK_OR(inner->isNamed());
                    auto& NTI = inner->asNamed();
                    MRDOCS_CHECK_OR(NTI.Name);
                    MRDOCS_CHECK_OR(NTI.Name->isSpecialization());
                    auto& SNI = NTI.Name->asSpecialization();
                    SNI.specializationID = SI->id;
                }();
            }

            // clang::CXXBaseSpecifier::getEllipsisLoc indicates whether the
            // base was a pack expansion; a PackExpansionType is not built
            // for base-specifiers
            MRDOCS_ASSERT(!BaseType.valueless_after_move());
            if (B.getEllipsisLoc().isValid())
            {
                BaseType->IsPackExpansion = true;
            }
            I.Bases.emplace_back(
                std::move(BaseType),
                toAccessKind(access),
                B.isVirtual());
        }
    }

    // Iterate over the friends of the class
    if (config_->extractFriends &&
        D->hasDefinition() &&
        D->hasFriends())
    {
        for (clang::FriendDecl const* FD : D->friends())
        {
            // Check if the friend is a fundamental type
            // Declaring a fundamental type like `int` as a friend of a
            // class or struct does not have any practical effect. Thus,
            // it's not considered part of the public API.
            if (clang::TypeSourceInfo const* TSI = FD->getFriendType())
            {
                clang::Type const* T = TSI->getType().getTypePtrOrNull();
                MRDOCS_CHECK_OR_CONTINUE(!T || !T->isBuiltinType());
            }
            FriendInfo F;
            populate(F, FD);
            if (F.id != SymbolID::invalid)
            {
                Symbol* FI = this->find(F.id);
                MRDOCS_CHECK_OR_CONTINUE(FI);
                MRDOCS_CHECK_OR_CONTINUE(FI->Extraction != ExtractionMode::ImplementationDefined);
            }
            auto it = std::ranges::find_if(I.Friends, [&](FriendInfo const& FI)
            {
                return FI.id == F.id;
            });
            if (it != I.Friends.end())
            {
                merge(*it, std::move(F));
            }
            else
            {
                I.Friends.push_back(std::move(F));
            }
        }
    }
}

void
ASTVisitor::
populate(RecordSymbol& I, clang::ClassTemplateDecl const* D)
{
    populate(I.Template, D->getTemplatedDecl(), D);
    populate(I, D->getTemplatedDecl());
}

void
ASTVisitor::
populate(RecordSymbol& I, clang::ClassTemplateSpecializationDecl const* D)
{
    populate(I.Template, D, D->getSpecializedTemplate());
    populate(I, cast<clang::CXXRecordDecl>(D));
}

void
ASTVisitor::
populate(RecordSymbol& I, clang::ClassTemplatePartialSpecializationDecl const* D)
{
    populate(I, dynamic_cast<clang::ClassTemplateSpecializationDecl const*>(D));
}

void
ASTVisitor::
populate(
    FunctionSymbol& I,
    clang::FunctionDecl const* D)
{
    MRDOCS_SYMBOL_TRACE(D, context_);

    // D is the templated declaration if FTD is non-null
    if (D->isFunctionTemplateSpecialization())
    {
        if (!I.Template)
        {
            I.Template.emplace();
        }

        if (auto* FTSI = D->getTemplateSpecializationInfo())
        {
            generateID(
                getInstantiatedFrom(FTSI->getTemplate()),
                I.Template->Primary);

            // TemplateArguments is used instead of TemplateArgumentsAsWritten
            // because explicit specializations of function templates may have
            // template arguments deduced from their return type and parameters
            if(auto* Args = FTSI->TemplateArguments)
            {
                populate(I.Template->Args, Args->asArray());
            }
        }
        else if (auto* DFTSI = D->getDependentSpecializationInfo())
        {
            // Only extract the ID of the primary template if there is
            // a single candidate primary template.
            if (auto Candidates = DFTSI->getCandidates(); Candidates.size() == 1)
            {
                generateID(getInstantiatedFrom(
                    Candidates.front()), I.Template->Primary);
            }
            if(auto* Args = DFTSI->TemplateArgumentsAsWritten)
            {
                populate(I.Template->Args, Args);
            }
        }
    }

    // Get the function type and extract information that comes from the type
    if (auto FT = getDeclaratorType(D); !FT.isNull())
    {
        MRDOCS_SYMBOL_TRACE(FT, context_);
        auto const* FPT = FT->template getAs<clang::FunctionProtoType>();
        MRDOCS_SYMBOL_TRACE(FPT, context_);
        populate(I.Noexcept, FPT);
        I.HasTrailingReturn |= FPT->hasTrailingReturn();
    }

    I.OverloadedOperator = toOperatorKind(D->getOverloadedOperator());
    I.IsVariadic |= D->isVariadic();
    I.IsDefaulted |= D->isDefaulted();
    I.IsExplicitlyDefaulted |= D->isExplicitlyDefaulted();
    I.IsDeleted |= D->isDeleted();
    I.IsDeletedAsWritten |= D->isDeletedAsWritten();
    I.IsNoReturn |= D->isNoReturn();
    I.HasOverrideAttr |= D->hasAttr<clang::OverrideAttr>();

    if (clang::ConstexprSpecKind const CSK = D->getConstexprKind();
        CSK != clang::ConstexprSpecKind::Unspecified)
    {
        I.Constexpr = toConstexprKind(CSK);
    }

    if (clang::StorageClass const SC = D->getStorageClass())
    {
        I.StorageClass = toStorageClassKind(SC);
    }

    I.IsNodiscard |= D->hasAttr<clang::WarnUnusedResultAttr>();
    I.IsExplicitObjectMemberFunction |= D->hasCXXExplicitFunctionObjectParameter();

    llvm::ArrayRef<clang::ParmVarDecl*> const params = D->parameters();
    I.Params.resize(params.size());
    for (std::size_t i = 0; i < params.size(); ++i)
    {
        clang::ParmVarDecl const* P = params[i];
        MRDOCS_SYMBOL_TRACE(P, context_);
        Param& param = I.Params[i];

        if (!param.Name && !P->getName().empty())
        {
            param.Name = P->getName();
        }

        if (param.Type->isAuto())
        {
            param.Type = toType(P->getOriginalType());
        }

        clang::Expr const* default_arg = P->hasUninstantiatedDefaultArg() ?
            P->getUninstantiatedDefaultArg() : P->getInit();
        if (!param.Default && default_arg)
        {
            param.Default = getSourceCode(default_arg->getSourceRange());
            if (param.Default)
            {
                param.Default = trim(*param.Default);
                if (param.Default->starts_with("= "))
                {
                    param.Default->erase(0, 2);
                    param.Default = ltrim(*param.Default);
                }
            }
        }
    }

    I.Class = toFunctionClass(D->getDeclKind());

    // extract the return type in direct dependency mode
    // if it contains a placeholder type which is
    // deduced as a local class type
    clang::QualType const RT = D->getReturnType();
    MRDOCS_SYMBOL_TRACE(RT, context_);
    I.ReturnType = toType(RT);

    if (auto* TRC = D->getTrailingRequiresClause().ConstraintExpr)
    {
        populate(I.Requires, TRC);
    }
    else if (I.Requires.Written.empty())
    {
        MRDOCS_ASSERT(!I.ReturnType.valueless_after_move());
        // Return type SFINAE constraints
        if (!I.ReturnType->Constraints.empty())
        {
            for (ExprInfo const& constraint: I.ReturnType->Constraints)
            {
                if (!I.Requires.Written.empty())
                {
                    I.Requires.Written += " && ";
                }
                I.Requires.Written += constraint.Written;
            }
        }

        // Iterate I.Params to find trailing requires clauses
        for (auto it = I.Params.begin(); it != I.Params.end(); )
        {
            MRDOCS_ASSERT(!it->Type.valueless_after_move());
            if (!it->Type->Constraints.empty())
            {
                for (ExprInfo const& constraint: it->Type->Constraints)
                {
                    if (!I.Requires.Written.empty())
                    {
                        I.Requires.Written += " && ";
                    }
                    I.Requires.Written += constraint.Written;
                }
                it = I.Params.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    populateAttributes(I, D);
}

void
ASTVisitor::
populate(FunctionSymbol& I, clang::FunctionTemplateDecl const* D)
{
    clang::FunctionDecl const* TD = D->getTemplatedDecl();
    populate(I.Template, TD, D);
    if (auto* C = dyn_cast<clang::CXXConstructorDecl>(TD))
    {
        populate(I, C);
    }
    else if (auto* Dtor = dyn_cast<clang::CXXDestructorDecl>(TD))
    {
        populate(I, Dtor);
    }
    else if (auto* Conv = dyn_cast<clang::CXXConversionDecl>(TD))
    {
        populate(I, Conv);
    }
    else if (auto* M = dyn_cast<clang::CXXMethodDecl>(TD))
    {
        populate(I, M);
    }
    else
    {
        populate(I, TD);
    }
}

void
ASTVisitor::
populate(FunctionSymbol& I, clang::CXXMethodDecl const* D)
{
    clang::FunctionDecl const* FD = D;
    populate(I, FD);
    I.IsRecordMethod = true;
    I.IsVirtual |= D->isVirtual();
    I.IsVirtualAsWritten |= D->isVirtualAsWritten();
    I.IsPure |= D->isPureVirtual();
    I.IsConst |= D->isConst();
    I.IsVolatile |= D->isVolatile();
    I.RefQualifier = toReferenceKind(D->getRefQualifier());
    I.IsFinal |= D->hasAttr<clang::FinalAttr>();
}

void
ASTVisitor::
populate(FunctionSymbol& I, clang::CXXConstructorDecl const* D)
{
    clang::CXXMethodDecl const* FD = D;
    populate(I, FD);
    populate(I.Explicit, D->getExplicitSpecifier());
}

void
ASTVisitor::
populate(FunctionSymbol& I, clang::CXXDestructorDecl const* D)
{
    clang::CXXMethodDecl const* FD = D;
    populate(I, FD);
}

void
ASTVisitor::
populate(FunctionSymbol& I, clang::CXXConversionDecl const* D)
{
    clang::CXXMethodDecl const* FD = D;
    populate(I, FD);
    populate(I.Explicit, D->getExplicitSpecifier());
}


void
ASTVisitor::
populate(
    EnumSymbol& I,
    clang::EnumDecl const* D)
{
    I.Scoped = D->isScoped();
    if (D->isFixed())
    {
        I.UnderlyingType = toType(D->getIntegerType());
    }
}

void
ASTVisitor::
populate(
    EnumConstantSymbol& I,
    clang::EnumConstantDecl const* D)
{
    I.Name = extractName(D);
    populate(
        I.Initializer,
        D->getInitExpr(),
        D->getInitVal());
}

void
ASTVisitor::
populate(TypedefSymbol& I, clang::TypedefNameDecl const* D)
{
    clang::QualType const QT = D->getUnderlyingType();
    I.Type = toType(QT);
}

void
ASTVisitor::
populate(TypedefSymbol& I, clang::TypedefDecl const* D)
{
    populate(I, cast<clang::TypedefNameDecl>(D));
}

void
ASTVisitor::
populate(TypedefSymbol& I, clang::TypeAliasDecl const* D)
{
    I.IsUsing = isa<clang::TypeAliasDecl>(D);
    populate(I, cast<clang::TypedefNameDecl>(D));
}

void
ASTVisitor::
populate(TypedefSymbol& I, clang::TypeAliasTemplateDecl const* D)
{
    populate(I.Template, D->getTemplatedDecl(), D);
    if (auto* TD = D->getTemplatedDecl();
        isa<clang::TypeAliasDecl>(TD))
    {
        populate(I, cast<clang::TypeAliasDecl>(TD));
    }
    else
    {
        populate(I, TD);
    }
}

void
ASTVisitor::
populate(
    VariableSymbol& I,
    clang::VarDecl const* D)
{
    // KRYSTIAN FIXME: we need to properly merge storage class
    if (clang::StorageClass const SC = D->getStorageClass())
    {
        I.StorageClass = toStorageClassKind(SC);
    }
    // this handles thread_local, as well as the C
    // __thread and __Thread_local specifiers
    I.IsThreadLocal |= D->getTSCSpec() !=
        clang::ThreadStorageClassSpecifier::TSCS_unspecified;
    // KRYSTIAN NOTE: VarDecl does not provide getConstexprKind,
    // nor does it use getConstexprKind to store whether
    // a variable is constexpr/constinit. Although
    // only one is permitted in a variable declaration,
    // it is possible to declare a static data member
    // as both constexpr and constinit in separate declarations..
    I.IsConstinit |= D->hasAttr<clang::ConstInitAttr>();
    I.IsConstexpr |= D->isConstexpr();
    I.IsInline |= D->isInline();
    if (clang::Expr const* E = D->getInit())
    {
        populate(I.Initializer, E);
    }
    auto QT = D->getType();
    if (D->isConstexpr()) {
        // when D->isConstexpr() is true, QT contains a redundant
        // `const` qualifier which we need to remove
        QT.removeLocalConst();
    }
    I.Type = toType(QT);
    populateAttributes(I, D);
}

void
ASTVisitor::
populate(VariableSymbol& I, clang::VarTemplateDecl const* D)
{
    populate(I.Template, D->getTemplatedDecl(), D);
    populate(I, D->getTemplatedDecl());
}

void
ASTVisitor::
populate(VariableSymbol& I, clang::VarTemplateSpecializationDecl const* D)
{
    populate(I.Template, D, D->getSpecializedTemplate());
    populate(I, cast<clang::VarDecl>(D));
}

void
ASTVisitor::
populate(VariableSymbol& I, clang::VarTemplatePartialSpecializationDecl const* D)
{
    populate(I, dynamic_cast<clang::VarTemplateSpecializationDecl const*>(D));
}

void
ASTVisitor::
populate(
    VariableSymbol& I,
    clang::FieldDecl const* D)
{
    I.IsRecordField = true;
    I.Type = toType(D->getType());
    if (clang::Expr const* E = D->getInClassInitializer())
    {
        populate(I.Initializer, E);
    }
    I.IsVariant = D->getParent()->isUnion();
    I.IsMutable = D->isMutable();
    if(D->isBitField())
    {
        I.IsBitfield = true;
        populate(I.BitfieldWidth, D->getBitWidth());
    }
    I.HasNoUniqueAddress = D->hasAttr<clang::NoUniqueAddressAttr>();
    I.IsDeprecated = D->hasAttr<clang::DeprecatedAttr>();
    I.IsMaybeUnused = D->hasAttr<clang::UnusedAttr>();
    populateAttributes(I, D);
}

void
ASTVisitor::
populate(
    FriendInfo& I,
    clang::FriendDecl const* D)
{
    if (clang::TypeSourceInfo const* TSI = D->getFriendType())
    {
        I.Type = toType(TSI->getType());
        MRDOCS_CHECK_OR((*I.Type)->isNamed());
        auto const& NTI = (*I.Type)->asNamed();
        MRDOCS_CHECK_OR(NTI.Name);
        I.id = NTI.Name->id;
    }
    else if (clang::NamedDecl const* ND = D->getFriendDecl())
    {
        // ND can be a function or a class
        ScopeExitRestore s(mode_, Dependency);
        if (Symbol const* SI = traverse(dyn_cast<clang::Decl>(ND)))
        {
            I.id = SI->id;
        }
    }
    // The newly traversed info might need to inherit the
    // documentation from the FriendDecl when the friend
    // is the only declaration.
    MRDOCS_CHECK_OR(isDocumented(D));
    Symbol* TI = find(I.id);
    MRDOCS_CHECK_OR(TI);
    MRDOCS_CHECK_OR(!TI->doc);
    populate(TI->doc, D);
}

void
ASTVisitor::
populate(
    GuideSymbol& I,
    clang::CXXDeductionGuideDecl const* D)
{
    I.Deduced = toType(D->getReturnType());
    for (clang::ParmVarDecl const* P : D->parameters())
    {
        I.Params.emplace_back(
            toType(P->getOriginalType()),
            P->getNameAsString(),
            // deduction guides cannot have default arguments
            std::string());
    }
    populate(I.Explicit, D->getExplicitSpecifier());
}

void
ASTVisitor::
populate(
    GuideSymbol& I,
    clang::FunctionTemplateDecl const* D)
{
    populate(I.Template, D->getTemplatedDecl(), D);
    populate(I, cast<clang::CXXDeductionGuideDecl>(D->getTemplatedDecl()));
}

void
ASTVisitor::
populate(
    NamespaceAliasSymbol& I,
    clang::NamespaceAliasDecl const* D)
{
    clang::NamedDecl const* Aliased = D->getAliasedNamespace();
    clang::NestedNameSpecifier NNS = D->getQualifier();
    Optional<Polymorphic<Name>> NI = toName(Aliased, {}, NNS);
    MRDOCS_CHECK_OR(NI);
    MRDOCS_ASSERT((*NI)->isIdentifier());
    I.AliasedSymbol = std::move((**NI).asIdentifier());
}

void
ASTVisitor::
populate(
    UsingSymbol& I,
    clang::UsingDecl const* D)
{
    I.Class = UsingClass::Normal;
    clang::DeclarationName const& Name = D->getNameInfo().getName();
    clang::NestedNameSpecifier const& NNS = D->getQualifier();
    auto INI = toName(Name, {}, NNS);
    MRDOCS_CHECK_OR(INI);
    I.IntroducedName = *INI;
    for (clang::UsingShadowDecl const* UDS: D->shadows())
    {
        ScopeExitRestore s(mode_, Dependency);
        clang::Decl* S = UDS->getTargetDecl();
        using enum ExtractionMode;
        if (Symbol* SI = findOrTraverse(S);
            SI &&
            !is_one_of(SI->Extraction,{Dependency, ImplementationDefined}))
        {
            I.ShadowDeclarations.emplace_back(SI->id);
        }
    }
}

void
ASTVisitor::
populate(
    ConceptSymbol& I,
    clang::ConceptDecl const* D)
{
    populate(I.Template, D, D);
    populate(I.Constraint, D->getConstraintExpr());
}


/*  Default function to populate template parameters
 */
template <
    std::derived_from<clang::Decl> DeclTy,
    std::derived_from<clang::TemplateDecl> TemplateDeclTy>
void
ASTVisitor::
populate(TemplateInfo& Template, DeclTy const*, TemplateDeclTy const* TD)
{
    MRDOCS_ASSERT(TD);
    MRDOCS_CHECK_OR(!TD->isImplicit());
    if (clang::TemplateParameterList const* TPL = TD->getTemplateParameters();
        !TPL->empty() &&
        std::ranges::none_of(TPL->asArray(), [](clang::NamedDecl const* ND) {
            return !ND->isImplicit();
        }))
    {
        return;
    }
    clang::TemplateParameterList const* TPL = TD->getTemplateParameters();
    populate(Template, TPL);
}

void
ASTVisitor::
populate(
    TemplateInfo& Template,
    clang::ClassTemplateSpecializationDecl const* CTSD,
    clang::ClassTemplateDecl const* CTD)
{
    MRDOCS_ASSERT(CTD);

    // If D is a partial/explicit specialization, extract the template arguments
    generateID(getInstantiatedFrom(CTD), Template.Primary);

    // Extract the template arguments of the specialization
    if (auto const* argsAsWritten = CTSD->getTemplateArgsAsWritten())
    {
        populate(Template.Args, argsAsWritten);
    }
    else
    {
        // Implicit specializations do not have template arguments as written
        populate(Template.Args, CTSD->getTemplateArgs().asArray());
    }

    // Extract requires clause from SFINAE context
    if (Template.Requires.Written.empty())
    {
        for (auto it = Template.Args.begin(); it != Template.Args.end();)
        {
            auto& arg = *it;
            MRDOCS_ASSERT(!arg.valueless_after_move());
            if (auto* T = (arg.operator->())->asTypePtr())
            {
                MRDOCS_ASSERT(!T->Type.valueless_after_move());
                if (!T->Type->Constraints.empty())
                {
                    for (ExprInfo const& constraint: T->Type->Constraints)
                    {
                        if (!Template.Requires.Written.empty())
                        {
                            Template.Requires.Written += " && ";
                        }
                        Template.Requires.Written += constraint.Written;
                    }
                    it = Template.Args.erase(it);
                    continue;
                }
            }
            ++it;
        }
    }


    // Extract the template parameters if this is a partial specialization
    if (auto* CTPSD = dyn_cast<clang::ClassTemplatePartialSpecializationDecl>(CTSD))
    {
        clang::TemplateParameterList* params = CTPSD->getTemplateParameters();
        populate(Template, params);
    }
}

template<std::derived_from<clang::VarDecl> VarDeclTy>
void
ASTVisitor::
populate(
    TemplateInfo& Template,
    VarDeclTy const* D,
    clang::VarTemplateDecl const* VTD)
{
    MRDOCS_ASSERT(VTD);

    // If D is a partial/explicit specialization, extract the template arguments
    if(auto* VTSD = dyn_cast<clang::VarTemplateSpecializationDecl>(D))
    {
        generateID(getInstantiatedFrom(VTD), Template.Primary);
        // extract the template arguments of the specialization
        populate(Template.Args, VTSD->getTemplateArgsAsWritten());
        // extract the template parameters if this is a partial specialization
        if (auto* VTPSD = dyn_cast<clang::VarTemplatePartialSpecializationDecl>(D))
        {
            populate(Template, VTPSD->getTemplateParameters());
        }
    }
    else
    {
        // otherwise, extract the template parameter list from VTD
        populate(Template, VTD->getTemplateParameters());
    }
}

void
ASTVisitor::
populate(
    NoexceptInfo& I,
    clang::FunctionProtoType const* FPT)
{
    MRDOCS_ASSERT(FPT);
    I.Implicit = ! FPT->hasNoexceptExceptionSpec();
    I.Kind = toNoexceptKind(
        FPT->getExceptionSpecType());
    // store the operand, if any
    if (clang::Expr const* NoexceptExpr = FPT->getNoexceptExpr())
    {
        I.Operand = toString(NoexceptExpr);
    }
}

void
ASTVisitor::
populate(
    ExplicitInfo& I,
    clang::ExplicitSpecifier const& ES)
{
    I.Implicit = ! ES.isSpecified();
    I.Kind = toExplicitKind(ES);

    // store the operand, if any
    if (clang::Expr const* ExplicitExpr = ES.getExpr())
    {
        I.Operand = toString(ExplicitExpr);
    }
}

void
ASTVisitor::
populate(
    ExprInfo& I,
    clang::Expr const* E)
{
    if (!E)
    {
        return;
    }
    I.Written = getSourceCode(
        E->getSourceRange());
}

template <class T>
void
ASTVisitor::
populate(
    ConstantExprInfo<T>& I,
    clang::Expr const* E)
{
    populate(static_cast<ExprInfo&>(I), E);
    // if the expression is dependent,
    // we cannot get its value
    if (!E || E->isValueDependent())
    {
        return;
    }
    I.Value.emplace(toInteger<T>(E->EvaluateKnownConstInt(context_)));
}

template <class T>
void
ASTVisitor::
populate(
    ConstantExprInfo<T>& I,
    clang::Expr const* E,
    llvm::APInt const& V)
{
    populate(I, E);
    I.Value.emplace(toInteger<T>(V));
}

template
void
ASTVisitor::
populate<std::uint64_t>(
    ConstantExprInfo<std::uint64_t>& I,
    clang::Expr const* E,
    llvm::APInt const& V);


void
ASTVisitor::
populate(
    Polymorphic<TParam>& I,
    clang::NamedDecl const* N)
{
    MRDOCS_ASSERT(!I.valueless_after_move());
    visit(N, [&]<typename DeclTy>(DeclTy const* P)
    {
        constexpr clang::Decl::Kind kind =
            DeclToKind<DeclTy>();

        if constexpr(kind == clang::Decl::TemplateTypeParm)
        {
            if (I->Kind != TParamKind::Type)
            {
                I = Polymorphic<TParam>(std::in_place_type<TypeTParam>);
            }
            auto* R = (I.operator->())->asTypePtr();
            if (P->wasDeclaredWithTypename())
            {
                R->KeyKind = TParamKeyKind::Typename;
            }
            if (P->hasDefaultArgument() && !R->Default)
            {
                R->Default = toTArg(
                    P->getDefaultArgument().getArgument());
            }
            if (clang::TypeConstraint const* TC = P->getTypeConstraint())
            {
                clang::NestedNameSpecifier NNS =
                    TC->getNestedNameSpecifierLoc().getNestedNameSpecifier();
                Optional<clang::ASTTemplateArgumentListInfo const*> TArgs;
                if (TC->hasExplicitTemplateArgs())
                {
                    TArgs.emplace(TC->getTemplateArgsAsWritten());
                }
                R->Constraint = toName(TC->getNamedConcept(), TArgs, NNS);
            }
            return;
        }
        else if constexpr(kind == clang::Decl::NonTypeTemplateParm)
        {
            if (I->Kind != TParamKind::Constant)
            {
                I = Polymorphic<TParam>(std::in_place_type<ConstantTParam>);
            }
            auto* R = (I.operator->())->asConstantPtr();
            R->Type = toType(P->getType());
            if (P->hasDefaultArgument() && !R->Default)
            {
                R->Default = toTArg(
                    P->getDefaultArgument().getArgument());
            }
            return;
        }
        else if constexpr(kind == clang::Decl::TemplateTemplateParm)
        {
            if (I->Kind != TParamKind::Template)
            {
                I = Polymorphic<TParam>(std::in_place_type<TemplateTParam>);
            }
            auto const* TTPD = cast<clang::TemplateTemplateParmDecl>(P);
            MRDOCS_CHECK_OR(TTPD);
            clang::TemplateParameterList const* TPL = TTPD->getTemplateParameters();
            MRDOCS_CHECK_OR(TPL);
            auto* Result = (I.operator->())->asTemplatePtr();
            Result->Params.reserve(TPL->size());
            for (std::size_t i = 0; i < TPL->size(); ++i)
            {
                clang::NamedDecl const* TP = TPL->getParam(i);
                auto& Param
                    = i < Result->Params.size() ?
                          Result->Params[i] :
                          Result->Params.emplace_back(std::in_place_type<TypeTParam>);
                populate(Param, TP);
            }
            if (TTPD->hasDefaultArgument() && !Result->Default)
            {
                clang::TemplateArgumentLoc const& TAL = TTPD->getDefaultArgument();
                clang::TemplateArgument const& TA = TAL.getArgument();
                Result->Default = toTArg(TA);
            }
            return;
        }
        MRDOCS_UNREACHABLE();
    });

    MRDOCS_ASSERT(!I.valueless_after_move());
    if (I->Name.empty())
    {
        I->Name = extractName(N);
    }
    // KRYSTIAN NOTE: Decl::isParameterPack
    // returns true for function parameter packs
    I->IsParameterPack =
        N->isTemplateParameterPack();
}

void
ASTVisitor::
populate(
    TemplateInfo& TI,
    clang::TemplateParameterList const* TPL)
{
    if (!TPL)
    {
        return;
    }
    auto TemplateParameters = TPL->asArray();
    auto ExplicitTemplateParameters =
        std::views::filter(
            TemplateParameters,
            [](clang::NamedDecl const* P)
            {
                return !P->isImplicit();
            });
    std::size_t const nExplicit = std::ranges::distance(
        ExplicitTemplateParameters);
    MRDOCS_CHECK_OR(nExplicit);
    TI.Params.reserve(nExplicit);
    auto explicitIt = ExplicitTemplateParameters.begin();
    std::size_t i = 0;
    while (explicitIt != ExplicitTemplateParameters.end())
    {
        clang::NamedDecl const* P = TPL->getParam(i);
        Polymorphic<TParam>& Param =
            i < TI.Params.size() ?
                TI.Params[i] :
                TI.Params.emplace_back(std::in_place_type<TypeTParam>);
        populate(Param, P);
        ++explicitIt;
        ++i;
    }
    if (auto* RC = TPL->getRequiresClause())
    {
        populate(TI.Requires, RC);
    }
    else if (TI.Requires.Written.empty())
    {
        // If there's no requires clause, check if the template
        // parameter types we extracted have constraints
        for (auto it = TI.Params.begin(); it != TI.Params.end(); )
        {
            Polymorphic<TParam>& param = *it;
            MRDOCS_ASSERT(!param.valueless_after_move());

            if (param->isConstant())
            {
                auto& T = (*param).asConstant();
                MRDOCS_ASSERT(!T.Type.valueless_after_move());
                if (!T.Type->Constraints.empty())
                {
                    for (ExprInfo const& constraint: T.Type->Constraints)
                    {
                        if (!TI.Requires.Written.empty())
                        {
                            TI.Requires.Written += " && ";
                        }
                        TI.Requires.Written += constraint.Written;
                    }
                    it = TI.Params.erase(it);
                    continue;
                }
            }

            if (param->Default &&
                (*param->Default)->isType())
            {
                if (auto const* T = ((*param->Default).operator->())->asTypePtr())
                {
                    MRDOCS_ASSERT(!T->Type.valueless_after_move());
                    if (!T->Type->Constraints.empty())
                    {
                        for (ExprInfo const& constraint: T->Type->Constraints)
                        {
                            if (!TI.Requires.Written.empty())
                            {
                                TI.Requires.Written += " && ";
                            }
                            TI.Requires.Written += constraint.Written;
                        }
                        it = TI.Params.erase(it);
                        continue;
                    }
                }
            }

            ++it;
        }
    }
}

void
ASTVisitor::
populate(
    std::vector<Polymorphic<TArg>>& result,
    clang::ASTTemplateArgumentListInfo const* args)
{
    if (!args)
    {
        return;
    }
    return populate(result,
        std::views::transform(args->arguments(),
            [](auto& x) -> auto&
        {
            return x.getArgument();
        }));
}

template <std::derived_from<Symbol> InfoTy>
void
ASTVisitor::
populateAttributes(InfoTy& I, clang::Decl const* D)
{
    if constexpr (requires { I.Attributes; })
    {
        MRDOCS_CHECK_OR(D->hasAttrs());
        for (clang::Attr const* attr: D->getAttrs())
        {
            clang::IdentifierInfo const* II = attr->getAttrName();
            if (!II)
            {
                continue;
            }
            if (!contains(I.Attributes, II->getName()))
            {
                I.Attributes.emplace_back(II->getName());
            }
        }
    }
}

void
ASTVisitor::
addMember(
    NamespaceSymbol& I, Symbol const& Member)
{
    if (auto const* U = Member.asNamespacePtr())
    {
        addMember(I.Members.Namespaces, *U);
        return;
    }
    if (auto const* U = Member.asNamespaceAliasPtr())
    {
        addMember(I.Members.NamespaceAliases, *U);
        return;
    }
    if (auto const* U = Member.asTypedefPtr())
    {
        addMember(I.Members.Typedefs, *U);
        return;
    }
    if (auto const* U = Member.asRecordPtr())
    {
        addMember(I.Members.Records, *U);
        return;
    }
    if (auto const* U = Member.asEnumPtr())
    {
        addMember(I.Members.Enums, *U);
        return;
    }
    if (auto const* U = Member.asFunctionPtr())
    {
        addMember(I.Members.Functions, *U);
        return;
    }
    if (auto const* U = Member.asVariablePtr())
    {
        addMember(I.Members.Variables, *U);
        return;
    }
    if (auto const* U = Member.asConceptPtr())
    {
        addMember(I.Members.Concepts, *U);
        return;
    }
    if (auto const* U = Member.asGuidePtr())
    {
        addMember(I.Members.Guides, *U);
        return;
    }
    if (auto const* U = Member.asUsingPtr())
    {
        addMember(I.Members.Usings, *U);
        return;
    }
    report::error("Cannot push {} of type {} into members of namespace {}",
        Member.Name,
        mrdocs::toString(Member.Kind).c_str(),
        I.Name);
}

void
ASTVisitor::
addMember(
    RecordSymbol& I, Symbol const& Member)
{
    switch (Member.Access)
    {
    case AccessKind::Public:
        addMember(I.Interface.Public, Member);
        break;
    case AccessKind::Private:
        addMember(I.Interface.Private, Member);
        break;
    case AccessKind::Protected:
        addMember(I.Interface.Protected, Member);
        break;
    default:
        MRDOCS_UNREACHABLE();
    }
}

void
ASTVisitor::
addMember(RecordTranche& T, Symbol const& Member)
{
    if (auto const* U = Member.asNamespaceAliasPtr())
    {
        addMember(T.NamespaceAliases, *U);
        return;
    }
    if (auto const* U = Member.asTypedefPtr())
    {
        addMember(T.Typedefs, *U);
        return;
    }
    if (auto const* U = Member.asRecordPtr())
    {
        addMember(T.Records, *U);
        return;
    }
    if (auto const* U = Member.asEnumPtr())
    {
        addMember(T.Enums, *U);
        return;
    }
    if (auto const* U = Member.asFunctionPtr())
    {
        if (U->StorageClass != StorageClassKind::Static)
        {
            addMember(T.Functions, *U);
        }
        else
        {
            addMember(T.StaticFunctions, *U);
        }
        return;
    }
    if (auto const* U = Member.asVariablePtr())
    {
        if (U->StorageClass != StorageClassKind::Static)
        {
            addMember(T.Variables, *U);
        }
        else
        {
            addMember(T.StaticVariables, *U);
        }
        return;
    }
    if (auto const* U = Member.asConceptPtr())
    {
        addMember(T.Concepts, *U);
        return;
    }
    if (auto const* U = Member.asGuidePtr())
    {
        addMember(T.Guides, *U);
        return;
    }
    if (auto const* U = Member.asUsingPtr())
    {
        addMember(T.Usings, *U);
        return;
    }
    report::error("Cannot push {} of type {} into tranche",
        Member.Name,
        mrdocs::toString(Member.Kind).c_str());
}

void
ASTVisitor::
addMember(EnumSymbol& I, Symbol const& Member) const
{
    if (auto const* U = Member.asEnumConstantPtr())
    {
        addMember(I.Constants, *U);
        return;
    }
    report::error("Cannot push {} of type {} into members of enum {}",
        Member.Name,
        mrdocs::toString(Member.Kind).c_str(),
        I.Name);
}

void
ASTVisitor::
addMember(OverloadsSymbol& I, Symbol const& Member) const
{
    if (Member.isFunction())
    {
        addMember(I.Members, Member);
        return;
    }
    report::error("Cannot push {} of type {} into members of enum {}",
        Member.Name,
        mrdocs::toString(Member.Kind).c_str(),
        I.Name);
}

void
ASTVisitor::
addMember(std::vector<SymbolID>& container, Symbol const& Member) const
{
    if (!contains(container, Member.id))
    {
        container.push_back(Member.id);
    }
}

template <std::derived_from<clang::Decl> DeclTy>
std::string
ASTVisitor::
extractName(DeclTy const* D)
{
    if constexpr (std::same_as<DeclTy, clang::CXXDeductionGuideDecl>)
    {
        return extractName(D->getDeducedTemplate());
    }
    else if constexpr (std::derived_from<DeclTy, clang::FriendDecl>)
    {
        if (auto* FD = D->getFriendDecl())
        {
            return extractName(D->getFriendDecl());
        }
        if (clang::TypeSourceInfo const* FT = D->getFriendType())
        {
            std::string Name;
            llvm::raw_string_ostream os(Name);
            FT->getType().print(os, context_.getPrintingPolicy());
            return Name;
        }
    }
    else if constexpr (std::derived_from<DeclTy, clang::UsingDirectiveDecl>)
    {
        return extractName(D->getNominatedNamespace());
    }
    else if constexpr (std::derived_from<DeclTy, clang::NamedDecl>)
    {
        return extractName(cast<clang::NamedDecl>(D));
    }
    return {};
}

std::string
ASTVisitor::
extractName(clang::NamedDecl const* D)
{
    return extractName(D->getDeclName());
}

std::string
ASTVisitor::
extractName(clang::DeclarationName const N)
{
    std::string result;
    if (N.isEmpty())
    {
        return result;
    }
    switch(N.getNameKind())
    {
    case clang::DeclarationName::Identifier:
        if (auto const* I = N.getAsIdentifierInfo())
        {
            result.append(I->getName());
        }
        break;
    case clang::DeclarationName::CXXDestructorName:
        result.push_back('~');
        [[fallthrough]];
    case clang::DeclarationName::CXXConstructorName:
        if (auto const* R = N.getCXXNameType()->getAsCXXRecordDecl())
        {
            result.append(R->getIdentifier()->getName());
        }
        break;
    case clang::DeclarationName::CXXDeductionGuideName:
        if (auto const* T = N.getCXXDeductionGuideTemplate())
        {
            result.append(T->getIdentifier()->getName());
        }
        break;
    case clang::DeclarationName::CXXConversionFunctionName:
    {
        result.append("operator ");
        // KRYSTIAN FIXME: we *really* should not be
        // converting types to strings like this
        result.append(mrdocs::toString(*toType(N.getCXXNameType())));
        break;
    }
    case clang::DeclarationName::CXXOperatorName:
    {
        OperatorKind const K = toOperatorKind(
            N.getCXXOverloadedOperator());
        result.append("operator");
        std::string_view const name = getOperatorName(K);
        if (std::isalpha(name.front()))
        {
            result.push_back(' ');
        }
        result.append(name);
        break;
    }
    case clang::DeclarationName::CXXLiteralOperatorName:
        case clang::DeclarationName::CXXUsingDirective:
        break;
    default:
        MRDOCS_UNREACHABLE();
    }
    return result;
}

llvm::SmallString<256>
ASTVisitor::
qualifiedName(clang::Decl const* D) const
{
    if (auto* ND = dyn_cast<clang::NamedDecl>(D))
    {
        return qualifiedName(ND);
    }
    return {};
}

llvm::SmallString<256>
ASTVisitor::
qualifiedName(clang::NamedDecl const* ND) const
{
    llvm::SmallString<256> name;
    llvm::raw_svector_ostream stream(name);
    getQualifiedName(ND, stream, context_.getPrintingPolicy());
    return name;
}

Polymorphic<Type>
ASTVisitor::
toType(clang::QualType const qt, TraversalMode const mode)
{
    MRDOCS_SYMBOL_TRACE(qt, context_);

    // The qualified symbol referenced by a regular symbol is a dependency.
    // For library types, can be proved wrong and the Info type promoted
    // to a regular type later on if the type matches the regular
    // extraction criteria
    ScopeExitRestore s(mode_, mode);

    // Build the Type representation for the type
    TypeBuilder Builder(*this);
    Builder.Visit(qt);
    return Builder.result();
}

Optional<Polymorphic<Name>>
ASTVisitor::
toName(clang::NestedNameSpecifier NNS)
{
    MRDOCS_SYMBOL_TRACE(NNS, context_);
    ScopeExitRestore scope(mode_, Dependency);
    switch(NNS.getKind()) {
    case clang::NestedNameSpecifier::Kind::Null:
        return std::nullopt;
    case clang::NestedNameSpecifier::Kind::Type: {
        const clang::Type *T = NNS.getAsType();
        NameBuilder Builder(*this);
        Builder.Visit(T);
        return Builder.result();
    }
    case clang::NestedNameSpecifier::Kind::Namespace: {
        auto I = Polymorphic<Name>(std::in_place_type<IdentifierName>);
        auto [ND, Prefix] = NNS.getAsNamespaceAndPrefix();
        I->Identifier = ND->getIdentifier()->getName();
        I->Prefix = toName(Prefix);
        clang::Decl const* ID = getInstantiatedFrom(ND);
        if (Symbol* info = findOrTraverse(const_cast<clang::Decl*>(ID)))
        {
            I->id = info->id;
        }
        return I;
    }
    case clang::NestedNameSpecifier::Kind::Global:
    case clang::NestedNameSpecifier::Kind::MicrosoftSuper:
    default:
        // Unimplemented
        return std::nullopt;
    }
    MRDOCS_UNREACHABLE();
}

template <class TArgRange>
Optional<Polymorphic<Name>>
ASTVisitor::
toName(
    clang::DeclarationName const Name,
    Optional<TArgRange> TArgs,
    clang::NestedNameSpecifier NNS)
{
    if (Name.isEmpty())
    {
        return std::nullopt;
    }
    Optional<Polymorphic<struct Name>> I = std::nullopt;
    if (TArgs)
    {
        I = Polymorphic<struct Name>(std::in_place_type<SpecializationName>);
        populate((**I).asSpecialization().TemplateArgs, *TArgs);
    }
    else
    {
        I = Polymorphic<struct Name>(std::in_place_type<IdentifierName>);
    }
    (*I)->Identifier = extractName(Name);
    (*I)->Prefix = toName(NNS);
    return I;
}

template <class TArgRange>
Optional<Polymorphic<Name>>
ASTVisitor::
toName(
    clang::Decl const* D,
    Optional<TArgRange> TArgs,
    clang::NestedNameSpecifier NNS)
{
    auto const* ND = dyn_cast_if_present<clang::NamedDecl>(D);
    if (!ND)
    {
        return std::nullopt;
    }
    Optional<Polymorphic<Name>> I = toName(ND->getDeclName(), std::move(TArgs), NNS);
    if (!I)
    {
        return std::nullopt;
    }
    MRDOCS_ASSERT(!I->valueless_after_move());
    ScopeExitRestore scope(mode_, Dependency);
    auto* ID = getInstantiatedFrom(D);
    if (Symbol const* info = findOrTraverse(const_cast<clang::Decl*>(ID)))
    {
        (*I)->id = info->id;
    }
    return I;
}

template
Optional<Polymorphic<Name>>
ASTVisitor::
toName<llvm::ArrayRef<clang::TemplateArgument>>(
    clang::Decl const* D,
    Optional<llvm::ArrayRef<clang::TemplateArgument>> TArgs,
    clang::NestedNameSpecifier NNS);

Polymorphic<TArg>
ASTVisitor::
toTArg(clang::TemplateArgument const& A)
{
    // TypePrinter generates an internal placeholder name (e.g. type-parameter-0-0)
    // for template type parameters used as arguments. it also cannonicalizes
    // types, which we do not want (although, PrintingPolicy has an option to change this).
    // thus, we use the template arguments as written.

    // KRYSTIAN NOTE: this can probably be changed to select
    // the argument as written when it is not dependent and is a type.
    // FIXME: constant folding behavior should be consistent with that of other
    // constructs, e.g. noexcept specifiers & explicit specifiers
    switch(A.getKind())
    {
    // empty template argument (e.g. not yet deduced)
    case clang::TemplateArgument::Null:
        break;

    // a template argument pack (any kind)
    case clang::TemplateArgument::Pack:
    {
        // we should never a clang::TemplateArgument::Pack here
        MRDOCS_UNREACHABLE();
        break;
    }
    // type
    case clang::TemplateArgument::Type:
    {
        auto R = Polymorphic<TArg>(std::in_place_type<TypeTArg>);
        clang::QualType QT = A.getAsType();
        MRDOCS_ASSERT(! QT.isNull());
        // if the template argument is a pack expansion,
        // use the expansion pattern as the type & mark
        // the template argument as a pack expansion
        if(clang::Type const* T = QT.getTypePtr();
            auto* PT = dyn_cast<clang::PackExpansionType>(T))
        {
            R->IsPackExpansion = true;
            QT = PT->getPattern();
        }
        static_cast<TypeTArg &>(*R).Type = toType(QT);
        return R;
    }
    // pack expansion of a template name
    case clang::TemplateArgument::TemplateExpansion:
    // template name
    case clang::TemplateArgument::Template:
    {
        auto R = Polymorphic<TArg>(std::in_place_type<TemplateTArg>);
        R->IsPackExpansion = A.isPackExpansion();
        std::string &Name = static_cast<TemplateTArg &>(*R).Name;

        // KRYSTIAN FIXME: template template arguments are
        // id-expression, so we don't properly support them yet.
        // for the time being, we will use the name & SymbolID of
        // the referenced declaration (if it isn't dependent),
        // and fallback to printing the template name otherwise
        clang::TemplateName const TN = A.getAsTemplateOrTemplatePattern();
        if(auto* TD = TN.getAsTemplateDecl())
        {
            if (auto* II = TD->getIdentifier())
            {
                Name = II->getName();
            }
        }
        else
        {
            llvm::raw_string_ostream stream(Name);
            TN.print(stream, context_.getPrintingPolicy(),
                clang::TemplateName::Qualified::AsWritten);
        }
        return R;
    }
    // nullptr value
    case clang::TemplateArgument::NullPtr:
    // expression referencing a declaration
    case clang::TemplateArgument::Declaration:
    // integral expression
    case clang::TemplateArgument::Integral:
    // expression
    case clang::TemplateArgument::Expression:
    {
        auto R = Polymorphic<TArg>(std::in_place_type<ConstantTArg>);
        R->IsPackExpansion = A.isPackExpansion();
        // if this is a pack expansion, use the template argument
        // expansion pattern in place of the template argument pack
        clang::TemplateArgument const& adjusted =
            R->IsPackExpansion ?
            A.getPackExpansionPattern() : A;

        llvm::raw_string_ostream stream(
            static_cast<ConstantTArg&>(*R).Value.Written);
        adjusted.print(context_.getPrintingPolicy(), stream, false);

        return Polymorphic<TArg>(R);
    }
    default:
        MRDOCS_UNREACHABLE();
    }
    return Polymorphic<TArg>(std::in_place_type<ConstantTArg>);
}


std::string
ASTVisitor::
toString(clang::Expr const* E)
{
    std::string result;
    llvm::raw_string_ostream stream(result);
    E->printPretty(stream, nullptr, context_.getPrintingPolicy());
    return result;
}

std::string
ASTVisitor::
toString(clang::Type const* T)
{
    if(auto* AT = dyn_cast_if_present<clang::AutoType>(T))
    {
        switch(AT->getKeyword())
        {
        case clang::AutoTypeKeyword::Auto:
        case clang::AutoTypeKeyword::GNUAutoType:
            return "auto";
        case clang::AutoTypeKeyword::DecltypeAuto:
            return "decltype(auto)";
        default:
            MRDOCS_UNREACHABLE();
        }
    }
    if(auto* TTPT = dyn_cast_if_present<clang::TemplateTypeParmType>(T))
    {
        if (clang::TemplateTypeParmDecl* TTPD = TTPT->getDecl();
            TTPD && TTPD->isImplicit())
        {
            return "auto";
        }
    }
    return clang::QualType(T, 0).getAsString(
        context_.getPrintingPolicy());
}

template<class Integer>
Integer
ASTVisitor::
toInteger(llvm::APInt const& V)
{
    if constexpr (std::is_signed_v<Integer>)
    {
        return static_cast<Integer>(V.getSExtValue());
    }
    else
    {
        return static_cast<Integer>(V.getZExtValue());
    }
}

std::string
ASTVisitor::
getSourceCode(clang::SourceRange const& R) const
{
    return clang::Lexer::getSourceText(
        clang::CharSourceRange::getTokenRange(R),
        source_,
        context_.getLangOpts()).str();
}

Optional<ASTVisitor::SFINAEInfo>
ASTVisitor::
extractSFINAEInfo(clang::QualType const T)
{
    MRDOCS_SYMBOL_TRACE(T, context_);
    MRDOCS_CHECK_OR(config_->sfinae, std::nullopt);

    // Get the primary template information of the type
    auto const templateInfo = getSFINAETemplateInfo(T, true);
    MRDOCS_CHECK_OR(templateInfo, std::nullopt);

    // Find the control parameters for SFINAE
    auto SFINAEControl = getSFINAEControlParams(*templateInfo);
    MRDOCS_CHECK_OR(SFINAEControl, std::nullopt);

    // Find the parameter that represents the SFINAE result
    auto const Args = templateInfo->Arguments;
    MRDOCS_SYMBOL_TRACE(Args, context_);
    auto const resultType = tryGetTemplateArgument(
        SFINAEControl->Parameters, Args, SFINAEControl->ParamIdx);
    MRDOCS_CHECK_OR(resultType, std::nullopt);
    MRDOCS_SYMBOL_TRACE(*resultType, context_);

    // Create a vector of template arguments that represent the
    // controlling parameters of the SFINAE template
    SFINAEInfo Result;
    Result.Type = resultType->getAsType();
    for (std::size_t I = 0; I < Args.size(); ++I)
    {
        if (SFINAEControl->ControllingParams[I])
        {
            MRDOCS_SYMBOL_TRACE(Args[I], context_);
            clang::TemplateArgument ArgsI = Args[I];
            MRDOCS_CHECK_OR_CONTINUE(ArgsI.getKind() == clang::TemplateArgument::ArgKind::Expression);
            clang::Expr* E = Args[I].getAsExpr();
            MRDOCS_CHECK_OR_CONTINUE(E);
            Result.Constraints.emplace_back();
            populate(Result.Constraints.back(), E);
        }
    }

    // Return the main type and controlling types
    return Result;
}

Optional<ASTVisitor::SFINAEControlParams>
ASTVisitor::
getSFINAEControlParams(
    clang::TemplateDecl* TD,
    clang::IdentifierInfo const* Member)
{
    MRDOCS_SYMBOL_TRACE(TD, context_);
    MRDOCS_SYMBOL_TRACE(Member, context_);
    MRDOCS_CHECK_OR(TD, std::nullopt);

    // The `FindParam` lambda function is used to find the index of a
    // template argument in a list of template arguments. It is used
    // to find the index of the controlling parameter in the list of
    // template arguments of the template declaration.
    auto FindParam = [this](
        llvm::ArrayRef<clang::TemplateArgument> Arguments,
        clang::TemplateArgument const& Arg) -> std::size_t
    {
        if (Arg.getKind() != clang::TemplateArgument::Type)
        {
            return -1;
        }
        auto const It = std::ranges::find_if(
            Arguments,
            [&](clang::TemplateArgument const& Other)
            {
                if (Other.getKind() != clang::TemplateArgument::Type)
                {
                    return false;
                }
                return context_.hasSameType(Other.getAsType(), Arg.getAsType());
            });
        bool const found = It != Arguments.end();
        return found ? It - Arguments.data() : static_cast<std::size_t>(-1);
    };

    if(auto* ATD = dyn_cast<clang::TypeAliasTemplateDecl>(TD))
    {
        // If the alias template is an alias template specialization,
        // we need to do the process for the underlying type
        MRDOCS_SYMBOL_TRACE(ATD, context_);
        auto Underlying = ATD->getTemplatedDecl()->getUnderlyingType();
        MRDOCS_SYMBOL_TRACE(Underlying, context_);
        auto underlyingTemplateInfo = getSFINAETemplateInfo(Underlying, Member == nullptr);
        MRDOCS_CHECK_OR(underlyingTemplateInfo, std::nullopt);
        if (Member)
        {
            // Get the member specified in the alias type from
            // the underlying type. If `Member` is `nullptr`,
            // `getSFINAETemplateInfo` was already allowed to populate
            // the `Member` field.
            underlyingTemplateInfo->Member = Member;
        }
        auto sfinaeControl = getSFINAEControlParams(*underlyingTemplateInfo);
        MRDOCS_CHECK_OR(sfinaeControl, std::nullopt);

        // Find the index of the parameter that represents the SFINAE result
        // in the underlying template arguments
        auto resultType = tryGetTemplateArgument(
            sfinaeControl->Parameters,
            underlyingTemplateInfo->Arguments,
            sfinaeControl->ParamIdx);
        MRDOCS_CHECK_OR(resultType, std::nullopt);
        MRDOCS_SYMBOL_TRACE(*resultType, context_);

        // Find the index of the parameter that represents the SFINAE result
        // in the primary template arguments
        unsigned ParamIdx = FindParam(ATD->getInjectedTemplateArgs(context_), *resultType);

        // Return the controlling parameters with values corresponding to
        // the primary template arguments
        clang::TemplateParameterList* primaryTemplParams = ATD->getTemplateParameters();
        MRDOCS_SYMBOL_TRACE(primaryTemplParams, context_);
        return SFINAEControlParams(
            primaryTemplParams,
            std::move(sfinaeControl->ControllingParams),
            ParamIdx);
    }

    // Ensure this is a clang::ClassTemplateDecl
    auto* CTD = dyn_cast<clang::ClassTemplateDecl>(TD);
    MRDOCS_SYMBOL_TRACE(CTD, context_);
    MRDOCS_CHECK_OR(CTD, std::nullopt);

    // Get the template arguments of the primary template
    auto PrimaryArgs = CTD->getInjectedTemplateArgs(context_);
    MRDOCS_SYMBOL_TRACE(PrimaryArgs, context_);

    // Type of the member that represents the SFINAE result.
    clang::QualType MemberType;

    // Index of the parameter that represents the the SFINAE result.
    // For instance, in the specialization `std::enable_if<true,T>::type`,
    // `type` is `T`, which corresponds to the second template parameter
    // `T`, so `ParamIdx` is `1` to represent the second parameter.
    unsigned ParamIdx = -1;

    // The `IsMismatch` function checks if there's a mismatch between the
    // clang::CXXRecordDecl of the clang::ClassTemplateDecl and the specified template
    // arguments. If there's a mismatch and `IsMismatch` returns `true`,
    // the caller returns `std::nullopt` to indicate that the template
    // is not a SFINAE template. If there are no mismatches, the caller
    // continues to check the controlling parameters of the template.
    // This function also updates the `MemberType` and `ParamIdx` variables
    // so that they can be used to check the controlling parameters.
    auto IsMismatch = [&](clang::CXXRecordDecl* RD, llvm::ArrayRef<clang::TemplateArgument> Args)
    {
        MRDOCS_SYMBOL_TRACE(RD, context_);
        MRDOCS_SYMBOL_TRACE(Args, context_);
        if (!RD->hasDefinition())
        {
            return false;
        }
        // Look for member in the record, such
        // as the member `::type` in `std::enable_if<B,T>`
        auto MemberLookup = RD->lookup(Member);
        MRDOCS_SYMBOL_TRACE(MemberLookup, context_);
        clang::QualType CurrentType;
        if(MemberLookup.empty())
        {
            if (!RD->getNumBases())
            {
                // Didn't find a definition for the specified member and
                // there can't be a base class that defines the
                // specified member: no mismatch
                return false;
            }
            for(auto& Base : RD->bases())
            {
                auto sfinae_info = getSFINAETemplateInfo(Base.getType(), false);
                if(! sfinae_info)
                {
                    // if the base is an opaque dependent type, we can't determine
                    // whether it's a SFINAE type
                    if (Base.getType()->isDependentType())
                    {
                        return true;
                    }
                    continue;
                }
                // if the class inherits from itself, we can't determine whether
                // it's a SFINAE type
                if (declaresSameEntity(TD, sfinae_info->Template))
                {
                    return true;
                }

                auto sfinae_result = getSFINAEControlParams(
                    sfinae_info->Template, Member);
                if (!sfinae_result)
                {
                    return true;
                }

                auto [template_params, controlling_params, param_idx] = *sfinae_result;
                auto resultType = tryGetTemplateArgument(
                    template_params, sfinae_info->Arguments, param_idx);
                if(! resultType)
                    return true;
                auto CurrentTypeFromBase = resultType->getAsType();
                if (CurrentType.isNull())
                {
                    CurrentType = CurrentTypeFromBase;
                }
                else if (
                    !context_.hasSameType(CurrentType, CurrentTypeFromBase))
                {
                    return true;
                }
            }
            // didn't find a base that defines the specified member
            if (CurrentType.isNull())
            {
                return false;
            }
        }
        else
        {
            // MemberLookup is not empty.
            if (!MemberLookup.isSingleResult())
            {
                // Ambiguous lookup: If there's more than one result,
                // we can't determine if the template is a SFINAE template
                // and return `true` to indicate that the template is not a
                // SFINAE template.
                return true;
            }
            if (auto* TND = dyn_cast<clang::TypedefNameDecl>(MemberLookup.front()))
            {
                // Update the current type to the underlying type of the
                // typedef declaration.
                // For instance, if the member is `::type` in the record
                // `std::enable_if<true,T>`, then the current type is `T`.
                // The next checks will occur for this underlying type.
                CurrentType = TND->getUnderlyingType();
                MRDOCS_SYMBOL_TRACE(CurrentType, context_);
            }
            else
            {
                // the specialization has a member with the right name,
                // but it isn't an alias declaration/typedef declaration...
                return true;
            }
        }

        // If the current type depends on a template parameter, we need to
        // find the corresponding template argument in the template arguments
        // of the primary template. If the template argument is not found,
        // we can't determine if the template is a SFINAE template and return
        // `true` to indicate a mismatch.
        if(CurrentType->isDependentType())
        {
            clang::TemplateArgument asTemplateArg(CurrentType);
            auto FoundIdx = FindParam(Args, asTemplateArg);
            if (FoundIdx == static_cast<std::size_t>(-1) ||
                FoundIdx >= PrimaryArgs.size())
            {
                return true;
            }
            // Set the controlling parameter index to the index of the
            // template argument that controls the SFINAE. For instance,
            // in the specialization `std::enable_if<true,T>::type`,
            // `type` is `T`, which corresponds to the second template
            // parameter `T`, so `ParamIdx` is `1` to represent the
            // second parameter.
            ParamIdx = FoundIdx;
            // Get this primary template argument as a template
            // argument of the current type.
            clang::TemplateArgument MappedPrimary = PrimaryArgs[FoundIdx];
            MRDOCS_SYMBOL_TRACE(MappedPrimary, context_);
            // The primary argument in SFINAE should be a type
            MRDOCS_ASSERT(MappedPrimary.getKind() == clang::TemplateArgument::Type);
            // Update the current type to the type of the primary argument
            CurrentType = MappedPrimary.getAsType();
            MRDOCS_SYMBOL_TRACE(CurrentType, context_);
        }

        // Update the type of the member that represents the SFINAE result
        // to the current type if it is not already set.
        if (MemberType.isNull())
        {
            MemberType = CurrentType;
        }

        // As a last check, the current type should be the same as the
        // type of the member that represents the SFINAE result so that
        // we can extract SFINAE information from the template.
        return !context_.hasSameType(MemberType, CurrentType);
    };

    // Check if there's a mismatch between the primary record and the arguments
    clang::CXXRecordDecl* PrimaryRD = CTD->getTemplatedDecl();
    MRDOCS_SYMBOL_TRACE(PrimaryRD, context_);
    MRDOCS_CHECK_OR(!IsMismatch(PrimaryRD, PrimaryArgs), std::nullopt);

    // Check if there's a mismatch between any explicit specialization and the arguments
    for(auto* CTSD : CTD->specializations())
    {
        MRDOCS_SYMBOL_TRACE(CTSD, context_);
        if (!CTSD->isExplicitSpecialization())
        {
            continue;
        }
        llvm::ArrayRef<clang::TemplateArgument> SpecArgs = CTSD->getTemplateArgs().asArray();
        MRDOCS_CHECK_OR(!IsMismatch(CTSD, SpecArgs), std::nullopt);
    }

    // Check if there's a mismatch between any partial specialization and the arguments
    llvm::SmallVector<clang::ClassTemplatePartialSpecializationDecl*> PartialSpecs;
    CTD->getPartialSpecializations(PartialSpecs);
    for(auto* CTPSD : PartialSpecs)
    {
        MRDOCS_SYMBOL_TRACE(CTPSD, context_);
        auto PartialArgs = CTPSD->getTemplateArgs().asArray();
        MRDOCS_SYMBOL_TRACE(PartialArgs, context_);
        MRDOCS_CHECK_OR(!IsMismatch(CTPSD, PartialArgs), std::nullopt);
    }

    // Find the controlling parameters of the template, that is, the
    // template parameters that control the SFINAE result. The controlling
    // parameters are expressions that cannot be converted to
    // non-type template parameters.
    llvm::SmallBitVector ControllingParams(PrimaryArgs.size());
    for(auto* CTPSD : PartialSpecs) {
        MRDOCS_SYMBOL_TRACE(CTPSD, context_);
        auto PartialArgs = CTPSD->getTemplateArgs().asArray();
        MRDOCS_SYMBOL_TRACE(PartialArgs, context_);
        for(std::size_t i = 0; i < PartialArgs.size(); ++i)
        {
            clang::TemplateArgument Arg = PartialArgs[i];
            MRDOCS_SYMBOL_TRACE(Arg, context_);
            switch (Arg.getKind())
            {
            case clang::TemplateArgument::Integral:
            case clang::TemplateArgument::Declaration:
            case clang::TemplateArgument::StructuralValue:
            case clang::TemplateArgument::NullPtr:
                break;
            case clang::TemplateArgument::Expression:
                if(getNTTPFromExpr(
                    Arg.getAsExpr(),
                    CTPSD->getTemplateDepth() - 1))
                    continue;
                break;
            default:
                continue;
            }
            ControllingParams.set(i);
        }
    }

    return SFINAEControlParams(CTD->getTemplateParameters(), std::move(ControllingParams), ParamIdx);
}

Optional<ASTVisitor::SFINAETemplateInfo>
ASTVisitor::getSFINAETemplateInfo(clang::QualType T, bool const AllowDependentNames) const
{
    MRDOCS_SYMBOL_TRACE(T, context_);
    MRDOCS_ASSERT(!T.isNull());

    // If the type is a dependent name type and dependent names are allowed,
    // extract the identifier and the qualifier's type
    SFINAETemplateInfo SFINAE;
    if (auto* DNT = T->getAsAdjusted<clang::DependentNameType>();
        DNT && AllowDependentNames)
    {
        SFINAE.Member = DNT->getIdentifier();
        MRDOCS_SYMBOL_TRACE(SFINAE.Member, context_);
        T = clang::QualType(DNT->getQualifier().getAsType(), 0);
        MRDOCS_SYMBOL_TRACE(T, context_);
    }
    if (!T.getTypePtrOrNull())
    {
        return std::nullopt;
    }

    // If the type is a template specialization type, extract the template name
    // and the template arguments
    if (auto* TST = T->getAsAdjusted<clang::TemplateSpecializationType>())
    {
        MRDOCS_SYMBOL_TRACE(TST, context_);
        SFINAE.Template = TST->getTemplateName().getAsTemplateDecl();
        MRDOCS_SYMBOL_TRACE(SFINAE.Template, context_);
        SFINAE.Arguments = TST->template_arguments();
        MRDOCS_SYMBOL_TRACE(SFINAE.Arguments, context_);
        return SFINAE;
    }

    // Return nullopt if the type does not match the expected patterns
    return std::nullopt;
}

Optional<clang::TemplateArgument>
ASTVisitor::
tryGetTemplateArgument(
    clang::TemplateParameterList* Parameters,
    llvm::ArrayRef<clang::TemplateArgument> const Arguments,
    std::size_t const Index)
{
    MRDOCS_SYMBOL_TRACE(Parameters, context_);
    MRDOCS_SYMBOL_TRACE(Arguments, context_);
    MRDOCS_CHECK_OR(Index != static_cast<unsigned>(-1), std::nullopt);

    // If the index is within the range of the template arguments, return the argument
    if (Index < Arguments.size())
    {
        return Arguments[Index];
    }

    MRDOCS_CHECK_OR(Parameters, std::nullopt);
    MRDOCS_CHECK_OR(Index < Parameters->size(), std::nullopt);

    // Attempt to get the default argument of the template parameter
    clang::NamedDecl* ND = Parameters->getParam(Index);
    MRDOCS_SYMBOL_TRACE(ND, context_);
    if(auto* TTPD = dyn_cast<clang::TemplateTypeParmDecl>(ND);
        TTPD && TTPD->hasDefaultArgument())
    {
        MRDOCS_SYMBOL_TRACE(TTPD, context_);
        return TTPD->getDefaultArgument().getArgument();
    }
    if(auto* NTTPD = dyn_cast<clang::NonTypeTemplateParmDecl>(ND);
        NTTPD && NTTPD->hasDefaultArgument())
    {
        MRDOCS_SYMBOL_TRACE(NTTPD, context_);
        return NTTPD->getDefaultArgument().getArgument();
    }
    return std::nullopt;
}

ExtractionMode
ASTVisitor::
checkFilters(
    clang::Decl const* D,
    clang::AccessSpecifier const access)
{
    if (mode_ == BaseClass &&
        isAnyImplicitSpecialization(D))
    {
        return ExtractionMode::Dependency;
    }

    // The translation unit is always extracted as the
    // global namespace. It can't fail any of the filters
    // because its qualified name is represented by the
    // empty string, and it has no file associated with it.
    MRDOCS_CHECK_OR(!isa<clang::TranslationUnitDecl>(D), ExtractionMode::Regular);

    // Check if this kind of symbol should be extracted.
    // This filters symbols supported by MrDocs and
    // symbol types whitelisted in the configuration,
    // such as private members and anonymous namespaces.
    MRDOCS_CHECK_OR(checkTypeFilters(D, access), ExtractionMode::Dependency);

    // Check if this symbol should be extracted according
    // to its qualified name. This checks if it matches
    // the symbol patterns and if it's not excluded.
    auto const [Cat, Kind] = checkSymbolFilters(D);
    if (Cat == ExtractionMode::Dependency)
    {
        return Cat;
    }

    // Check if this symbol should be extracted according
    // to its location. This checks if it's in one of the
    // input directories, if it matches the file patterns,
    // and it's not in an excluded file.
    MRDOCS_CHECK_OR(checkFileFilters(D), ExtractionMode::Dependency);

    return Cat;
}

bool
ASTVisitor::
checkTypeFilters(clang::Decl const* D, clang::AccessSpecifier const access)
{
    if (access == clang::AS_private)
    {
        // Don't extract private members
        if (isVirtualMember(D))
        {
            // Don't extract private virtual members
            return config_->extractPrivateVirtual || config_->extractPrivate;
        }
        return config_->extractPrivate;
    }
    if (!config_->extractAnonymousNamespaces)
    {
        // Don't extract anonymous namespaces
        MRDOCS_CHECK_OR(!isAnonymousNamespace(D), false);
    }
    if (!config_->extractStatic)
    {
        MRDOCS_CHECK_OR(!isStaticFileLevelMember(D), false);
    }
    if (!config_->extractLocalClasses && isa<clang::RecordDecl>(D))
    {
        if (auto const* FI = findFileInfo(D);
            FI->full_path.ends_with(".cpp") ||
            FI->full_path.ends_with(".cc") ||
            FI->full_path.ends_with(".cxx") ||
            FI->full_path.ends_with(".c"))
        {
            return false;
        }
    }

    // Don't extract anonymous unions
    auto const* RD = dyn_cast<clang::RecordDecl>(D);
    MRDOCS_CHECK_OR(!RD || !RD->isAnonymousStructOrUnion(), false);

    // Don't extract declarations implicitly generated by the compiler
    MRDOCS_CHECK_OR(!D->isImplicit() || isa<clang::IndirectFieldDecl>(D), false);

    return true;
}

bool
ASTVisitor::
checkFileFilters(clang::Decl const* D)
{
    MRDOCS_SYMBOL_TRACE(D, context_);

    FileInfo* fileInfo = findFileInfo(D);
    MRDOCS_CHECK_OR(fileInfo, false);

    // Check pre-processed file filters
    MRDOCS_CHECK_OR(!fileInfo->passesFilters, *fileInfo->passesFilters);

    // Call the non-cached version of this function
    bool const ok = checkFileFilters(fileInfo->full_path);

    // Add to cache
    fileInfo->passesFilters.emplace(ok);
    return *fileInfo->passesFilters;
}

bool
ASTVisitor::
checkFileFilters(std::string_view const symbolPath) const
{
    // Don't extract declarations that fail the input filter
    auto startsWithSymbolPath = [&](std::string const& inputDir)
    {
        return files::startsWith(symbolPath, inputDir);
    };
    if (config_->recursive)
    {
        MRDOCS_CHECK_OR(
            config_->input.empty() ||
            std::ranges::any_of(config_->input, startsWithSymbolPath),
            false);
    }
    else
    {
        MRDOCS_CHECK_OR(
            config_->input.empty() ||
            std::ranges::any_of(config_->input,
                [symbolParentDir = files::getParentDir(symbolPath)]
                (std::string const& inputDir)
                {
                    return inputDir == symbolParentDir;
                }),
            false);
    }

    // Don't extract declarations that fail the exclude filter
    MRDOCS_CHECK_OR(
        config_->exclude.empty() ||
        std::ranges::none_of(config_->exclude, startsWithSymbolPath),
        false);

    // Don't extract declarations that fail the exclude pattern filter
    MRDOCS_CHECK_OR(
        config_->excludePatterns.empty() ||
        std::ranges::none_of(config_->excludePatterns,
            [&](PathGlobPattern const& pattern)
            {
                return pattern.match(symbolPath);
            }),
        false);

    // Don't extract declarations that fail the file pattern filter
    MRDOCS_CHECK_OR(
        config_->filePatterns.empty() ||
        std::ranges::any_of(config_->filePatterns,
        [symbolFilename = files::getFileName(symbolPath)]
        (PathGlobPattern const& pattern)
            {
                return pattern.match(symbolFilename);
            }),
        false);

    return true;
}

ASTVisitor::ExtractionInfo
ASTVisitor::
checkSymbolFilters(clang::Decl const* D, bool const AllowParent)
{
    // Use the cache
    if (auto const it = extraction_.find(D); it != extraction_.end())
    {
        return it->second;
    }

    // Update cache
    auto updateCache = [this, D](ExtractionInfo const result) {
        extraction_.emplace(D, result);
        return result;
    };

    // If not a clang::NamedDecl, then symbol filters don't apply
    auto const* ND = dyn_cast<clang::NamedDecl>(D);
    if (!ND)
    {
        constexpr ExtractionInfo res{ExtractionMode::Regular, ExtractionMatchType::Strict};
        return updateCache(res);
    }

    // Get the symbol name
    llvm::SmallString<256> const name = qualifiedName(ND);
    auto const symbolName = name.str();

    // Function to check if parent is of a certain extraction mode
    auto ParentIs = [&](clang::Decl const* D, ExtractionMode expected) {
        if (clang::Decl const* P = getParent(D);
            P &&
            !isa<clang::TranslationUnitDecl>(P))
        {
            auto const [parentMode, kind] = checkSymbolFilters(P);
            return parentMode == expected;
        }
        return false;
    };

    // 0) We should check the exclusion filters first. If a symbol is
    // explicitly excluded, there's nothing else to check.
    if (!config_->excludeSymbols.empty())
    {
        if (checkSymbolFiltersImpl<Strict>(config_->excludeSymbols, symbolName))
        {
            ExtractionInfo const res{ExtractionMode::Dependency, ExtractionMatchType::Strict};
            return updateCache(res);
        }

        // 0a) Check if the parent is excluded
        if (AllowParent &&
            ParentIs(D, ExtractionMode::Dependency))
        {
            return updateCache({ExtractionMode::Dependency, ExtractionMatchType::StrictParent});
        }
    }

    // If not excluded, we should check the filters in this order:
    // - implementation-defined
    // - see-below
    // - include-symbols
    // These filters have precedence over each other.
    std::array const patternsAndModes = {
        std::make_pair(&config_->implementationDefined, ExtractionMode::ImplementationDefined),
        std::make_pair(&config_->seeBelow, ExtractionMode::SeeBelow),
        std::make_pair(&config_->includeSymbols, ExtractionMode::Regular)
    };

    // 1) The symbol strictly matches one of the patterns
    for (auto const& [patterns, patternsMode] : patternsAndModes)
    {
        MRDOCS_CHECK_OR_CONTINUE(!patterns->empty());
        if (checkSymbolFiltersImpl<Strict>(*patterns, symbolName))
        {
            ExtractionInfo const res = {patternsMode, ExtractionMatchType::Strict};
            return updateCache(res);
        }

        // 1a) Check if the parent in the same exclusion category
        // (see-below or implementation defined).
        MRDOCS_CHECK_OR_CONTINUE(AllowParent);
        MRDOCS_CHECK_OR_CONTINUE(patternsMode != ExtractionMode::Regular);
        MRDOCS_CHECK_OR_CONTINUE(ParentIs(D, patternsMode));
        if (patternsMode == ExtractionMode::ImplementationDefined)
        {
            // A child of implementation defined is also
            // implementation defined.
            return updateCache(
                { ExtractionMode::ImplementationDefined,
                  ExtractionMatchType::StrictParent });
        }
        if (patternsMode == ExtractionMode::SeeBelow)
        {
            // A child of see-below is also see-below (if namespace)
            // or dependency (if record)
            if (clang::Decl const* P = getParent(D);
                P &&
                isa<clang::NamespaceDecl>(P))
            {
                return updateCache(
                { ExtractionMode::SeeBelow,
                  ExtractionMatchType::StrictParent });
            }
            return updateCache(
                { ExtractionMode::Dependency,
                  ExtractionMatchType::StrictParent });
        }
    }

    // 2) A namespace where the symbol is defined matches one of the
    // literal patterns in `include-symbols`.
    // For instance, if the literal pattern `std` is in `include-symbols`,
    // then `std::filesystem::path::iterator` is extracted even though
    // the pattern only matches `std`.
    // In other words, because `std` is a namespace and `std` is a
    // literal pattern, it matches all symbols in the `std` namespace
    // and its subnamespaces as if the pattern were `std::**`.
    // 2a) Check if there are any literal patterns in the filters.
    // This is an optimization to avoid checking the parent namespaces
    // if there are no literal patterns in the filters.
    bool const containsLiteralPatterns = std::ranges::any_of(patternsAndModes,
        [&](auto const& v)
        {
            auto& [patterns, mode] = v;
            return std::ranges::any_of(*patterns, [](auto const& pattern)
            {
                return pattern.isLiteral();
            });
        });
    if (containsLiteralPatterns)
    {
        // 2b) For each parent namespace
        clang::Decl const* Cur = getParent(D);
        while (Cur)
        {
            if (isa<clang::NamespaceDecl>(Cur))
            {
                // 2c) Check if it matches any literal pattern
                llvm::SmallString<256> const namespaceName = qualifiedName(Cur);
                for (auto const& [patterns, mode] : patternsAndModes)
                {
                    if (!patterns->empty() &&
                        checkSymbolFiltersImpl<Literal>(*patterns, namespaceName.str()))
                    {
                        ExtractionInfo const res = {mode, ExtractionMatchType::LiteralParent};
                        return updateCache(res);
                    }
                }
            }
            Cur = getParent(Cur);
        }
    }

    // 3) Child symbols imply this symbol should be included
    // If symbol is a namespace, the namespace is the parent of a symbol that matches
    // one of the patterns in the filters.
    // For instance, if `std::filesystem::*` is in `include-symbols`, then `std`
    // and `std::filesystem` are extracted even though `std::` and `std::filesystem::`
    // only match the prefix of the pattern.
    // In other words, including `std::filesystem::*` implies `std` and `std::filesystem`
    // should be included.
    // We evaluate this rule in the reverse order of precedence of the filters
    // because, for instance, if a namespace matches as a prefix for `include-symbol`
    // and `implementation-defined`, we should extract it as `include-symbol`,
    // since symbols that only pass `include-symbol` will also be included in this namespace
    // later on.
    if (isa<clang::NamespaceDecl>(D) || isa<clang::TranslationUnitDecl>(D))
    {
        llvm::SmallString<256> symbolAsPrefix{ symbolName };
        symbolAsPrefix += "::";
        for (auto const& [patterns, mode] : std::ranges::views::reverse(patternsAndModes))
        {
            MRDOCS_CHECK_OR_CONTINUE(!patterns->empty());
            MRDOCS_CHECK_OR_CONTINUE(
                checkSymbolFiltersImpl<
                    PrefixOnly>(*patterns, symbolAsPrefix.str()));
            // We know this namespace matches one of the pattern
            // prefixes that can potentially include children, but
            // we have to check if any children actually matches
            // the pattern strictly.
            auto const* DC = cast<clang::DeclContext>(D);
            auto childrenMode = ExtractionMode::Dependency;
            for (auto* M : DC->decls())
            {
                MRDOCS_SYMBOL_TRACE(M, context_);
                if (M->isImplicit() && !isa<clang::IndirectFieldDecl>(M))
                {
                    // Ignore implicit members
                    continue;
                }
                if (getParent(M) != D)
                {
                    // Not a semantic member
                    continue;
                }
                auto const [childMode, childKind] = checkSymbolFilters(M, false);
                if (childMode == ExtractionMode::Dependency)
                {
                    // The child should not be extracted.
                    // Go to next child.
                    continue;
                }
                if (childrenMode == ExtractionMode::Dependency)
                {
                    // Still a dependency. Initialize it with child mode.
                    childrenMode = childMode;
                }
                else
                {
                    // Children mode already initialized. Get the least specific one.
                    childrenMode = leastSpecific(childrenMode, childMode);
                }
                if (childrenMode == ExtractionMode::Regular)
                {
                    // Already the least specific
                    break;
                }
            }
            if (childrenMode != ExtractionMode::Dependency)
            {
                ExtractionInfo const res = {childrenMode, ExtractionMatchType::Prefix};
                return updateCache(res);
            }
        }
    }
    else if (AllowParent)
    {
        clang::Decl const* P = getParent(D);
        if (P)
        {
            // 4) Parent symbols imply this symbol should be included
            // If the first record, enum, or namespace parent of the symbol
            // matches one of the patterns, we extract the symbol in the same mode.
            // For instance, if `std::*` is in `include-symbols`, then
            // `std::vector::iterator` is extracted even though the
            // pattern only matches `std::vector`.
            // In other words, including `std::vector` implies
            // `std::vector::iterator` should be included.
            // This operates recursively, which will already update
            // the cache with the proper extraction mode for this parent.
            if (auto const [mode, kind] = checkSymbolFilters(P);
                mode != ExtractionMode::Dependency &&
                kind != ExtractionMatchType::Prefix)
            {
                // The parent is being extracted and the reason
                // is not because it's a prefix.
                // When it's a prefix, the parent is only
                // being extracted so that symbols that match
                // the full pattern are included and not all symbols.
                ExtractionInfo const res = {mode, ExtractionMatchType::StrictParent};
                return updateCache(res);
            }
        }
    }

    // 4) It doesn't match any of the filters
    // 4a) If this happened because there are no include-symbol
    // filters, we assume the `include-symbol` works as if
    // `**` is included instead of nothing being included.
    // Thus, we should extract the symbol.
    if (config_->includeSymbols.empty())
    {
        constexpr ExtractionInfo res = {ExtractionMode::Regular, ExtractionMatchType::Strict};
        return updateCache(res);
    }

    // 4b) Otherwise, we don't extract the symbol
    // because it doesn't match any of `include-symbol` filters
    constexpr ExtractionInfo res = {ExtractionMode::Dependency, ExtractionMatchType::Strict};
    return updateCache(res);
}

template <ASTVisitor::SymbolCheckType t>
bool
ASTVisitor::
checkSymbolFiltersImpl(
    std::vector<SymbolGlobPattern> const& patterns,
    std::string_view const symbolName) const
{
    // Don't extract declarations that fail the symbol filter
    auto includeMatchFn = [&](SymbolGlobPattern const& pattern)
    {
        if constexpr (t == SymbolCheckType::PrefixOnly)
        {
            // If the symbol is a scope, such as a namespace or class,
            // we want to know if symbols in that scope might match
            // the filters rather than the scope symbol itself.
            // Because if symbols in that scope match the filter, we also
            // want to extract the scope itself.
            // Thus, we only need to show we might potentially match one
            // of the prefixes of the symbol patterns, not the entire
            // symbol pattern for the escope.
            return pattern.matchPatternPrefix(symbolName);
        }
        else if constexpr (t == SymbolCheckType::Literal)
        {
            return pattern.isLiteral() && pattern.match(symbolName);
        }
        else if constexpr (t == SymbolCheckType::Strict)
        {
            // Strict match
            return pattern.match(symbolName);
        }
    };
    MRDOCS_CHECK_OR(
        std::ranges::any_of(patterns, includeMatchFn), false);

    return true;
}


Symbol*
ASTVisitor::
find(SymbolID const& id) const
{
    if (auto const it = info_.find(id); it != info_.end())
    {
        return it->get();
    }
    return nullptr;
}

Symbol*
ASTVisitor::
find(clang::Decl const* D) const
{
    auto ID = generateID(D);
    MRDOCS_CHECK_OR(ID, nullptr);
    return find(ID);
}

ASTVisitor::FileInfo*
ASTVisitor::
findFileInfo(clang::SourceLocation const loc)
{
    MRDOCS_CHECK_OR(!loc.isInvalid(), nullptr);
    // Find the presumed location, ignoring #line directives
    clang::PresumedLoc presumed = source_.getPresumedLoc(loc, false);
    clang::FileID id = presumed.getFileID();
    if(id.isInvalid())
        return nullptr;

    // Find in the cache
    if(auto const it = files_.find(id); it != files_.end())
        return std::addressof(it->second);

    auto [it, inserted] = files_.try_emplace(
        id, buildFileInfo(presumed.getFilename()));
    return std::addressof(it->second);
}

ASTVisitor::FileInfo*
ASTVisitor::
findFileInfo(clang::Decl const* D)
{
    clang::SourceLocation Loc = D->getBeginLoc();
    if (Loc.isInvalid())
    {
        Loc = D->getLocation();
    }
    return findFileInfo(Loc);
}

ASTVisitor::FileInfo
ASTVisitor::
buildFileInfo(std::string_view path)
{
    FileInfo file_info;
    file_info.full_path = path;

    if (! files::isAbsolute(file_info.full_path))
    {
        bool found = false;
        for (auto& includePath: config_->includes)
        {
            // append full path to this include path
            // and check if the file exists
            std::string fullPath = files::makeAbsolute(
                    file_info.full_path, includePath);
            if (files::exists(fullPath))
            {
                file_info.full_path = fullPath;
                found = true;
                break;
            }
        }
        if (!found)
        {
            file_info.full_path = files::makeAbsolute(
                file_info.full_path, config_->sourceRoot);
        }
    }

    if (!files::isPosixStyle(file_info.full_path))
    {
        file_info.full_path = files::makePosixStyle(file_info.full_path);
    }

    // Attempts to get a relative path for the prefix
    auto tryGetRelativePosixPath = [&file_info](std::string_view const prefix)
        -> Optional<std::string_view>
    {
        if (files::startsWith(file_info.full_path, prefix))
        {
            std::string_view res = file_info.full_path;
            res.remove_prefix(prefix.size());
            if (res.starts_with('/'))
            {
                res.remove_prefix(1);
            }
            return res;
        }
        return std::nullopt;
    };

    auto tryGetRelativePath = [&tryGetRelativePosixPath](std::string_view const prefix)
        -> Optional<std::string_view>
    {
        if (!files::isAbsolute(prefix))
        {
            return std::nullopt;
        }
        if (files::isPosixStyle(prefix))
        {
            // If already posix, we use the string view directly
            // to avoid creating a new string for the check
            return tryGetRelativePosixPath(prefix);
        }
        std::string const posixPrefix = files::makePosixStyle(prefix);
        return tryGetRelativePosixPath(posixPrefix);
    };

    // Populate file relative to source-root
    if (files::isAbsolute(config_->sourceRoot))
    {
        if (auto shortPath = tryGetRelativePath(config_->sourceRoot))
        {
            file_info.source_path = std::string(*shortPath);
        }
    }

    // Find the best match for the file path in the search directories
    for (clang::HeaderSearch& HS = sema_.getPreprocessor().getHeaderSearchInfo();
         clang::DirectoryLookup const& DL : HS.search_dir_range())
    {
        clang::OptionalDirectoryEntryRef DR = DL.getDirRef();
        if (!DL.isNormalDir() || !DR)
        {
            // Only consider normal directories
            continue;
        }
        clang::StringRef searchDir = DR->getName();
        if (auto shortPath = tryGetRelativePath(searchDir))
        {
            file_info.short_path = std::string(*shortPath);
            return file_info;
        }
    }

    // Fallback to the source root
    if (!file_info.source_path.empty())
    {
        file_info.short_path = file_info.source_path;
        return file_info;
    }

    // Fallback to system search paths in PATH
    Optional<std::string> const optEnvPathsStr = llvm::sys::Process::GetEnv("PATH");
    MRDOCS_CHECK_OR(optEnvPathsStr, file_info);
    std::string const& envPathsStr = *optEnvPathsStr;
    for (auto const envPaths = llvm::split(envPathsStr, llvm::sys::EnvPathSeparator);
         auto envPath: envPaths)
    {
        if (!files::isAbsolute(envPath))
        {
            continue;
        }
        if (auto shortPath = tryGetRelativePath(envPath))
        {
            file_info.short_path = std::string(*shortPath);
            return file_info;
        }
    }

    // Fallback to the full path
    file_info.short_path = file_info.full_path;
    return file_info;
}

template <std::derived_from<Symbol> InfoTy>
ASTVisitor::upsertResult<InfoTy>
ASTVisitor::
upsert(SymbolID const& id)
{
    // Creating symbol with invalid SymbolID
    MRDOCS_ASSERT(id != SymbolID::invalid);
    Symbol* info = find(id);
    bool const isNew = !info;
    if (!info)
    {
        info = info_.emplace(std::make_unique<InfoTy>(id)).first->get();
        auto const minExtract = mode_ == TraversalMode::Regular ?
            ExtractionMode::Regular : ExtractionMode::Dependency;
        info->Extraction = mostSpecific(info->Extraction, minExtract);
    }
    MRDOCS_ASSERT(info->Kind == InfoTy::kind_id);
    return {static_cast<InfoTy&>(*info), isNew};
}

template <
    class InfoTy,
    HasInfoTypeFor DeclType>
Expected<
    ASTVisitor::upsertResult<
        std::conditional_t<
            std::same_as<InfoTy, void>,
            InfoTypeFor_t<DeclType>,
            InfoTy>>>
ASTVisitor::
upsert(DeclType const* D)
{
    using R = std::conditional_t<
        std::same_as<InfoTy, void>,
        InfoTypeFor_t<DeclType>,
        InfoTy>;

    ExtractionMode const m = checkFilters(D);
    if (m == ExtractionMode::Dependency)
    {
        // If the extraction mode is dependency, it means we
        // should extract it as a dependency or that we
        // should not extract it.
        if (mode_ == Regular)
        {
            return Unexpected(Error("Symbol should not be extracted"));
        }
        if (isAnyExplicitSpecialization(D))
        {
            // We should not extract explicit specializations in dependency mode.
            // As this is a dependency, the corpus only needs to store the main
            // template.
            // The calling code should handle this case instead of
            // populating the symbol table with instantiations.
            return Unexpected(Error("Specialization in dependency mode"));
        }
    }

    SymbolID const id = generateID(D);
    MRDOCS_TRY(checkUndocumented<R>(id, D));

    MRDOCS_CHECK_MSG(id, "Failed to extract symbol ID");
    auto [I, isNew] = upsert<R>(id);

    // Already populate the extraction mode
    if (isNew)
    {
        I.Extraction = m;
    }
    else
    {
        I.Extraction = leastSpecific(I.Extraction, m);
    }

    // Already populate the access specifier
    clang::AccessSpecifier const access = getAccess(D);
    I.Access = toAccessKind(access);

    return upsertResult<R>{std::ref(I), isNew};
}

template <
    std::derived_from<Symbol> InfoTy,
    std::derived_from<clang::Decl> DeclTy>
Expected<void>
ASTVisitor::
checkUndocumented(
    SymbolID const& id,
    DeclTy const* D)
{
    // If `extract-all` is enabled, we don't need to
    // check for undocumented symbols
    MRDOCS_CHECK_OR(!config_->extractAll, {});
    // If the symbol is a namespace, the `extract-all`
    // doesn't apply to it
    MRDOCS_CHECK_OR((!std::same_as<InfoTy,NamespaceSymbol>), {});
    // If the symbol is not being extracted as a Regular
    // symbol, we don't need to check for undocumented symbols
    // These are expected to be potentially undocumented
    MRDOCS_CHECK_OR(mode_ == Regular, {});
    // Check if the symbol is documented, ensure this symbol is not in the set
    // of undocumented symbols in this translation unit and return
    // without an error if it is
    if (isDocumented(D))
    {
        if (config_->warnIfUndocumented)
        {
            auto const it = undocumented_.find(id);
            undocumented_.erase(it);
        }
        return {};
    }
    // If the symbol is undocumented, check if we haven't seen a
    // documented version before.
    if (auto const infoIt = info_.find(id);
        infoIt != info_.end() &&
        infoIt->get()->doc)
    {
        return {};
    }
    // If the symbol is undocumented, and we haven't seen a documented
    // version before, store this symbol in the set of undocumented
    // symbols we've seen so far in this translation unit.
    if (config_->warnIfUndocumented)
    {
        auto const undocIt = undocumented_.find(id);
        if (undocIt == undocumented_.end())
        {
            SymbolKind const kind = InfoTy::kind_id;
            undocumented_.insert(UndocumentedSymbol{id, extractName(D), kind});
        }
        // Populate the location
        auto handle = undocumented_.extract(undocIt);
        UndocumentedSymbol& UI = handle.value();
        populate(UI.Loc, D);
        undocumented_.insert(std::move(handle));
    }
    return Unexpected(Error("Undocumented"));
}

} // mrdocs
