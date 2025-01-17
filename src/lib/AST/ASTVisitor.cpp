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

#include "lib/AST/ASTVisitor.hpp"
#include "lib/AST/NameInfoBuilder.hpp"
#include "lib/AST/ClangHelpers.hpp"
#include "lib/AST/ParseJavadoc.hpp"
#include "lib/AST/TypeInfoBuilder.hpp"
#include "lib/Support/Path.hpp"
#include "lib/Support/Debug.hpp"
#include "lib/Support/Glob.hpp"
#include "lib/Lib/Diagnostics.hpp"
#include <mrdocs/Metadata.hpp>
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
#include <llvm/Support/Path.h>
#include <llvm/Support/SHA1.h>
#include <llvm/Support/Process.h>
#include <memory>
#include <optional>
#include <ranges>
#include <unordered_set>

namespace clang::mrdocs {

ASTVisitor::
ASTVisitor(
    const ConfigImpl& config,
    Diagnostics& diags,
    CompilerInstance& compiler,
    ASTContext& context,
    Sema& sema) noexcept
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
    // top-level TranslationUnitDecl.
    // If this `assert` fires, then it means
    // ASTContext::setTraversalScope is being (erroneously)
    // used somewhere
    MRDOCS_ASSERT(context_.getTraversalScope() ==
        std::vector<Decl*>{context_.getTranslationUnitDecl()});
}

void
ASTVisitor::
build()
{
    // traverse the translation unit, only extracting
    // declarations which satisfy all filter conditions.
    // dependencies will be tracked, but not extracted
    auto* TU = context_.getTranslationUnitDecl();
    traverse(TU);
    MRDOCS_ASSERT(find(SymbolID::global));
}

template <
    class InfoTy,
    std::derived_from<Decl> DeclTy>
Info*
ASTVisitor::
traverse(DeclTy* D)
{
    MRDOCS_ASSERT(D);
    MRDOCS_CHECK_OR(!D->isInvalidDecl(), nullptr);
    MRDOCS_SYMBOL_TRACE(D, context_);

    if constexpr (std::same_as<DeclTy, Decl>)
    {
        // Convert to the most derived type of the Decl
        // and call the appropriate traverse function
        return visit(D, [&]<typename DeclTyU>(DeclTyU* U) -> Info*
        {
            if constexpr (!std::same_as<DeclTyU, Decl>)
            {
                return traverse(U);
            }
            return nullptr;
        });
    }
    else if constexpr (HasInfoTypeFor<DeclTy> || std::derived_from<InfoTy, Info>)
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
        auto& [I, isNew] = *exp;

        // Populate the base classes with the necessary information.
        // Even when the object is new, we want to update the source locations
        // and the documentation status.
        populateInfoBases(I, isNew, D);

        // Populate the derived Info object with the necessary information
        // when the object is new. If the object already exists, this
        // information would be redundant.
        populate(I, D);

        // Traverse the members of the declaration according to the
        // current extraction mode.
        traverseMembers(I, D);

        // Traverse the parents of the declaration in dependency mode.
        traverseParents(I, D);

        return &I;
    }
    return nullptr;
}

Info*
ASTVisitor::
traverse(FunctionTemplateDecl* D)
{
    // Route the traversal to GuideInfo or FunctionInfo
    if (FunctionDecl* FD = D->getTemplatedDecl();
        FD && isa<CXXDeductionGuideDecl>(FD))
    {
        return traverse<GuideInfo>(D);
    }
    return traverse<FunctionInfo>(D);
}

Info*
ASTVisitor::
traverse(UsingDirectiveDecl* D)
{
    // Find the parent namespace
    ScopeExitRestore s1(mode_, ExtractionMode::Dependency);
    Decl* P = getParent(D);
    MRDOCS_SYMBOL_TRACE(P, context_);
    Info* PI = findOrTraverse(P);
    MRDOCS_CHECK_OR(PI, nullptr);
    auto PNI = dynamic_cast<NamespaceInfo*>(PI);
    MRDOCS_CHECK_OR(PNI, nullptr);

    // Find the nominated namespace
    Decl* ND = D->getNominatedNamespace();
    MRDOCS_SYMBOL_TRACE(ND, context_);
    ScopeExitRestore s2(mode_, ExtractionMode::Dependency);
    Info* NDI = findOrTraverse(ND);
    MRDOCS_CHECK_OR(NDI, nullptr);

    if (std::ranges::find(PNI->UsingDirectives, NDI->id) == PNI->UsingDirectives.end())
    {
        PNI->UsingDirectives.emplace_back(NDI->id);
    }
    return nullptr;
}

Info*
ASTVisitor::
traverse(IndirectFieldDecl* D)
{
    return traverse(D->getAnonField());
}

template <
    std::derived_from<Info> InfoTy,
    std::derived_from<Decl> DeclTy>
requires (!std::derived_from<DeclTy, RedeclarableTemplateDecl>)
void
ASTVisitor::
traverseMembers(InfoTy& I, DeclTy* DC)
{
    // When a declaration context is a function, we should
    // not traverse its members as function arguments are
    // not main Info members.
    if constexpr (
        !std::derived_from<DeclTy, FunctionDecl> &&
        std::derived_from<DeclTy, DeclContext>)
    {
        // We only need members of regular symbols and see-below namespaces
        // - SeeBelow symbols (that are not namespaces) are only the
        //   symbol and the documentation.
        // - ImplementationDefined symbols have no pages
        // - Dependency symbols only need the names
        MRDOCS_CHECK_OR(
            I.Extraction == ExtractionMode::Regular ||
            I.Extraction == ExtractionMode::SeeBelow);
        MRDOCS_CHECK_OR(
            I.Extraction != ExtractionMode::SeeBelow ||
            I.Kind == InfoKind::Namespace);

        // Set the context for the members
        ScopeExitRestore s(mode_, I.Extraction);

        // There are many implicit declarations in the
        // translation unit declaration, so we preemtively
        // skip them here.
        auto explicitMembers = std::ranges::views::filter(DC->decls(), [](Decl* D)
            {
                return !D->isImplicit() || isa<IndirectFieldDecl>(D);
            });
        for (auto* D : explicitMembers)
        {
            traverse(D);
        }
    }
}

template <
    std::derived_from<Info> InfoTy,
    std::derived_from<RedeclarableTemplateDecl> DeclTy>
void
ASTVisitor::
traverseMembers(InfoTy& I, DeclTy* D)
{
    traverseMembers(I, D->getTemplatedDecl());
}

template <
    std::derived_from<Info> InfoTy,
    std::derived_from<Decl> DeclTy>
requires (!std::derived_from<DeclTy, RedeclarableTemplateDecl>)
void
ASTVisitor::
traverseParents(InfoTy& I, DeclTy* DC)
{
    MRDOCS_SYMBOL_TRACE(DC, context_);
    if (Decl* PD = getParent(DC))
    {
        MRDOCS_SYMBOL_TRACE(PD, context_);

        // Check if we haven't already extracted or started
        // to extract the parent scope


        // Traverse the parent scope as a dependency if it
        // hasn't been extracted yet
        Info* PI = nullptr;
        {
            ScopeExitRestore s(mode_, ExtractionMode::Dependency);
            if (PI = findOrTraverse(PD); !PI)
            {
                return;
            }
        }

        // If we found the parent scope, set it as the parent
        I.Parent = PI->id;
        if (auto* SI = dynamic_cast<ScopeInfo*>(PI))
        {
            addMember(*SI, I);
        }
    }
}

template <
    std::derived_from<Info> InfoTy,
    std::derived_from<RedeclarableTemplateDecl> DeclTy>
void
ASTVisitor::
traverseParents(InfoTy& I, DeclTy* D)
{
    traverseParents(I, D->getTemplatedDecl());
}


Expected<llvm::SmallString<128>>
ASTVisitor::
generateUSR(const Decl* D) const
{
    MRDOCS_ASSERT(D);
    llvm::SmallString<128> res;

    if (auto const* NAD = dyn_cast<NamespaceAliasDecl>(D))
    {
        if (index::generateUSRForDecl(cast<Decl>(NAD->getNamespace()), res))
        {
            return Unexpected(Error("Failed to generate USR"));
        }
        res.append("@NA");
        res.append(NAD->getNameAsString());
        return res;
    }

    // Handling UsingDecl
    if (auto const* UD = dyn_cast<UsingDecl>(D))
    {
        for (const auto* shadow : UD->shadows())
        {
            if (index::generateUSRForDecl(shadow->getTargetDecl(), res))
            {
                return Unexpected(Error("Failed to generate USR"));
            }
        }
        res.append("@UDec");
        res.append(UD->getQualifiedNameAsString());
        return res;
    }

    if (auto const* UD = dyn_cast<UsingDirectiveDecl>(D))
    {
        if (index::generateUSRForDecl(UD->getNominatedNamespace(), res))
        {
            return Unexpected(Error("Failed to generate USR"));
        }
        res.append("@UDDec");
        res.append(UD->getQualifiedNameAsString());
        return res;
    }

    // Handling UnresolvedUsingTypenameDecl
    if (auto const* UD = dyn_cast<UnresolvedUsingTypenameDecl>(D))
    {
        if (index::generateUSRForDecl(UD, res))
        {
            return Unexpected(Error("Failed to generate USR"));
        }
        res.append("@UUTDec");
        res.append(UD->getQualifiedNameAsString());
        return res;
    }

    // Handling UnresolvedUsingValueDecl
    if (auto const* UD = dyn_cast<UnresolvedUsingValueDecl>(D))
    {
        if (index::generateUSRForDecl(UD, res))
        {
            return Unexpected(Error("Failed to generate USR"));
        }
        res.append("@UUV");
        res.append(UD->getQualifiedNameAsString());
        return res;
    }

    // Handling UsingPackDecl
    if (auto const* UD = dyn_cast<UsingPackDecl>(D))
    {
        if (index::generateUSRForDecl(UD, res))
        {
            return Unexpected(Error("Failed to generate USR"));
        }
        res.append("@UPD");
        res.append(UD->getQualifiedNameAsString());
        return res;
    }

    // Handling UsingEnumDecl
    if (auto const* UD = dyn_cast<UsingEnumDecl>(D))
    {
        if (index::generateUSRForDecl(UD, res))
        {
            return Unexpected(Error("Failed to generate USR"));
        }
        res.append("@UED");
        EnumDecl const* ED = UD->getEnumDecl();
        res.append(ED->getQualifiedNameAsString());
        return res;
    }

    // KRYSTIAN NOTE: clang doesn't currently support
    // generating USRs for friend declarations, so we
    // will improvise until I can merge a patch which
    // adds support for them
    if(auto const* FD = dyn_cast<FriendDecl>(D))
    {
        // first, generate the USR for the containing class
        if (index::generateUSRForDecl(cast<Decl>(FD->getDeclContext()), res))
        {
            return Unexpected(Error("Failed to generate USR"));
        }
        // add a seperator for uniqueness
        res.append("@FD");
        // if the friend declaration names a type,
        // use the USR generator for types
        if (TypeSourceInfo* TSI = FD->getFriendType())
        {
            if (index::generateUSRForType(TSI->getType(), context_, res))
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

    if (index::generateUSRForDecl(D, res))
        return Unexpected(Error("Failed to generate USR"));

    const auto* Described = dyn_cast_if_present<TemplateDecl>(D);
    const auto* Templated = D;
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
        const TemplateParameterList* TPL = Described->getTemplateParameters();
        if(const auto* RC = TPL->getRequiresClause())
        {
            RC = SubstituteConstraintExpressionWithoutSatisfaction(
                sema_, cast<NamedDecl>(isa<FunctionTemplateDecl>(Described) ? Described : Templated), RC);
            if (!RC)
            {
                return Unexpected(Error("Failed to generate USR"));
            }
            ODRHash odr_hash;
            odr_hash.AddStmt(RC);
            res.append("@TPL#");
            res.append(llvm::itostr(odr_hash.CalculateHash()));
        }
    }

    if(auto* FD = dyn_cast<FunctionDecl>(Templated);
        FD && FD->getTrailingRequiresClause())
    {
        const Expr* RC = FD->getTrailingRequiresClause();
        RC = SubstituteConstraintExpressionWithoutSatisfaction(
            sema_, cast<NamedDecl>(Described ? Described : Templated), RC);
        if (!RC)
        {
            return Unexpected(Error("Failed to generate USR"));
        }
        ODRHash odr_hash;
        odr_hash.AddStmt(RC);
        res.append("@TRC#");
        res.append(llvm::itostr(odr_hash.CalculateHash()));
    }

    return res;
}

bool
ASTVisitor::
generateID(
    const Decl* D,
    SymbolID& id) const
{
    if (!D)
    {
        return false;
    }

    if (isa<TranslationUnitDecl>(D))
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
generateID(const Decl* D) const
{
    SymbolID id = SymbolID::invalid;
    generateID(D, id);
    return id;
}

template <std::derived_from<Info> InfoTy, class DeclTy>
void
ASTVisitor::
populateInfoBases(InfoTy& I, bool const isNew, DeclTy* D)
{
    // Populate the documentation
    bool const isDocumented = generateJavadoc(I.javadoc, D);

    // Populate the source info
    if constexpr (std::derived_from<InfoTy, SourceInfo>)
    {
        bool const isDefinition = [&]() {
            if constexpr (requires {D->isThisDeclarationADefinition();})
            {
                return D->isThisDeclarationADefinition();
            }
            else
            {
                return false;
            }
        }();
        clang::SourceLocation Loc = D->getBeginLoc();
        if (Loc.isInvalid())
        {
            Loc = D->getLocation();
        }
        if (Loc.isValid())
        {
            populate(I, Loc, isDefinition, isDocumented);
        }
    }

    // If a symbol is extracted as a new dependency, check if
    // the symbol passes the include filters and already promote
    // it to regular.
    if (isNew &&
        I.Extraction == ExtractionMode::Dependency)
    {
        // Try an exact match here
        auto qualifiedName = this->qualifiedName(D);
        if (checkSymbolFiltersImpl<false>(std::string_view(qualifiedName.str())) &&
            checkFileFilters(D))
        {
            I.Extraction = ExtractionMode::Regular;
            // default mode also becomes regular for its
            // members
            mode_ = ExtractionMode::Regular;
        }
    }

    // Determine the reason why the symbol is being extracted
    if (I.Extraction == ExtractionMode::Regular ||
        I.Extraction == ExtractionMode::SeeBelow)
    {
        auto matchQualifiedName =
            [qualifiedName = this->qualifiedName(D)]
            (SymbolGlobPattern const& pattern)
            {
                std::string_view const qualifiedNameSV = qualifiedName.str();
                return pattern.match(std::string_view(qualifiedNameSV));
            };

        // Promote the extraction mode to SeeBelow if the symbol
        // matches any of the patterns or if any of its parents
        // are being extracted in SeeBelow mode
        if (I.Extraction == ExtractionMode::Regular &&
            !config_->seeBelow.empty())
        {
            if (std::ranges::any_of(config_->seeBelow, matchQualifiedName))
            {
                I.Extraction = ExtractionMode::SeeBelow;
            }
            else if (I.Parent)
            {
                Info const* PI = find(I.Parent);
                while (PI)
                {
                    if (PI->Extraction == ExtractionMode::SeeBelow)
                    {
                        I.Extraction = ExtractionMode::SeeBelow;
                        break;
                    }
                    PI = find(PI->Parent);
                }
            }
            else
            {
                auto* PD = getParent(D);
                Info const* PI = findOrTraverse(PD);
                while (PI)
                {
                    if (PI->Extraction == ExtractionMode::SeeBelow)
                    {
                        I.Extraction = ExtractionMode::SeeBelow;
                        break;
                    }
                    PD = getParent(PD);
                    PI = find(PD);
                }
            }
        }

        // Promote the extraction mode to ImplementationDefined if the symbol
        // matches any of the patterns or if any of its parents
        // are being extracted in ImplementationDefined mode
        if (!config_->implementationDefined.empty())
        {
            if (std::ranges::any_of(config_->implementationDefined, matchQualifiedName))
            {
                I.Extraction = ExtractionMode::ImplementationDefined;
            }
            else if (I.Parent)
            {
                Info const* PI = find(I.Parent);
                while (PI)
                {
                    if (PI->Extraction == ExtractionMode::ImplementationDefined)
                    {
                        I.Extraction = ExtractionMode::ImplementationDefined;
                        break;
                    }
                    PI = find(PI->Parent);
                }
            }
            else
            {
                auto* PD = getParent(D);
                Info const* PI = findOrTraverse(PD);
                while (PI)
                {
                    if (PI->Extraction == ExtractionMode::ImplementationDefined)
                    {
                        I.Extraction = ExtractionMode::ImplementationDefined;
                        break;
                    }
                    PD = getParent(PD);
                    PI = find(PD);
                }
            }
        }
    }

    // All other information is redundant if the symbol is not new
    MRDOCS_CHECK_OR(isNew);

    // These should already have been populated by traverseImpl
    MRDOCS_ASSERT(I.id);
    MRDOCS_ASSERT(I.Kind != InfoKind::None);

    if constexpr (std::same_as<DeclTy, CXXDeductionGuideDecl>)
    {
        I.Name = extractName(D->getDeducedTemplate());
    }
    else if constexpr (std::derived_from<DeclTy, FriendDecl>)
    {
        if (auto* FD = D->getFriendDecl())
        {
            I.Name = extractName(D->getFriendDecl());
        }
        else if (TypeSourceInfo const* FT = D->getFriendType())
        {
            llvm::raw_string_ostream os(I.Name);
            FT->getType().print(os, context_.getPrintingPolicy());
        }
    }
    else if constexpr (std::derived_from<DeclTy, UsingDirectiveDecl>)
    {
        I.Name = extractName(D->getNominatedNamespace());
    }
    else if constexpr (std::derived_from<DeclTy, NamedDecl>)
    {
        I.Name = extractName(D);
    }
}

void
ASTVisitor::
populate(
    SourceInfo& I,
    clang::SourceLocation const loc,
    bool const definition,
    bool const documented)
{
    unsigned line = source_.getPresumedLoc(
        loc, false).getLine();
    FileInfo* file = findFileInfo(loc);
    MRDOCS_ASSERT(file);

    if (definition)
    {
        if (I.DefLoc)
        {
            return;
        }
        I.DefLoc.emplace(file->full_path, file->short_path, file->source_path, line, documented);
    }
    else
    {
        auto const existing = std::ranges::
            find_if(I.Loc,
            [line, file](const Location& l)
            {
                return l.LineNumber == line &&
                    l.FullPath == file->full_path;
            });
        if (existing != I.Loc.end())
        {
            return;
        }
        I.Loc.emplace_back(file->full_path, file->short_path, file->source_path, line, documented);
    }
}

void
ASTVisitor::
populate(
    NamespaceInfo& I,
    NamespaceDecl* D)
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
    NamespaceInfo& I,
    TranslationUnitDecl*)
{
    I.id = SymbolID::global;
    I.IsAnonymous = false;
    I.IsInline = false;
}

void
ASTVisitor::
populate(
    RecordInfo& I,
    CXXRecordDecl* D)
{
    if (D->getTypedefNameForAnonDecl())
    {
        I.IsTypeDef = true;
    }
    I.KeyKind = toRecordKeyKind(D->getTagKind());
    // These are from CXXRecordDecl::isEffectivelyFinal()
    I.IsFinal = D->hasAttr<FinalAttr>();
    if (auto const* DT = D->getDestructor())
    {
        I.IsFinalDestructor = DT->hasAttr<FinalAttr>();
    }

    // Extract direct bases. D->bases() will get the bases
    // from whichever declaration is the definition (if any)
    if(D->hasDefinition() && I.Bases.empty())
    {
        for (const CXXBaseSpecifier& B : D->bases())
        {
            AccessSpecifier const access = B.getAccessSpecifier();

            if (!config_->privateBases &&
                access == AS_private)
            {
                continue;
            }

            QualType const BT = B.getType();
            auto BaseType = toTypeInfo(BT);

            // CXXBaseSpecifier::getEllipsisLoc indicates whether the
            // base was a pack expansion; a PackExpansionType is not built
            // for base-specifiers
            if (BaseType && B.getEllipsisLoc().isValid())
            {
                BaseType->IsPackExpansion = true;
            }
            I.Bases.emplace_back(
                std::move(BaseType),
                toAccessKind(access),
                B.isVirtual());
        }
    }
}

void
ASTVisitor::
populate(RecordInfo& I, ClassTemplateDecl* D)
{
    populate(I.Template, D->getTemplatedDecl(), D);
    populate(I, D->getTemplatedDecl());
}

void
ASTVisitor::
populate(RecordInfo& I, ClassTemplateSpecializationDecl* D)
{
    populate(I.Template, D, D->getSpecializedTemplate());
    populate(I, cast<CXXRecordDecl>(D));
}

template <std::derived_from<FunctionDecl> DeclTy>
void
ASTVisitor::
populate(
    FunctionInfo& I,
    DeclTy* D)
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
            generateID(getInstantiatedFrom(
                FTSI->getTemplate()), I.Template->Primary);

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
        const auto* FPT = FT->template getAs<FunctionProtoType>();
        MRDOCS_SYMBOL_TRACE(FPT, context_);
        populate(I.Noexcept, FPT);
        I.HasTrailingReturn |= FPT->hasTrailingReturn();
    }

    //
    // FunctionDecl
    //
    FunctionDecl const* FD = D;
    I.OverloadedOperator = toOperatorKind(FD->getOverloadedOperator());
    I.IsVariadic |= FD->isVariadic();
    I.IsDefaulted |= FD->isDefaulted();
    I.IsExplicitlyDefaulted |= FD->isExplicitlyDefaulted();
    I.IsDeleted |= FD->isDeleted();
    I.IsDeletedAsWritten |= FD->isDeletedAsWritten();
    I.IsNoReturn |= FD->isNoReturn();
    I.HasOverrideAttr |= FD->template hasAttr<OverrideAttr>();

    if (ConstexprSpecKind const CSK = FD->getConstexprKind();
        CSK != ConstexprSpecKind::Unspecified)
    {
        I.Constexpr = toConstexprKind(CSK);
    }

    if (StorageClass const SC = FD->getStorageClass())
    {
        I.StorageClass = toStorageClassKind(SC);
    }

    I.IsNodiscard |= FD->template hasAttr<WarnUnusedResultAttr>();
    I.IsExplicitObjectMemberFunction |= FD->hasCXXExplicitFunctionObjectParameter();

    //
    // CXXMethodDecl
    //
    if constexpr(std::derived_from<DeclTy, CXXMethodDecl>)
    {
        CXXMethodDecl const* MD = D;
        I.IsVirtual |= MD->isVirtual();
        I.IsVirtualAsWritten |= MD->isVirtualAsWritten();
        I.IsPure |= MD->isPureVirtual();
        I.IsConst |= MD->isConst();
        I.IsVolatile |= MD->isVolatile();
        I.RefQualifier = toReferenceKind(MD->getRefQualifier());
        I.IsFinal |= MD->template hasAttr<FinalAttr>();
        //MD->isCopyAssignmentOperator()
        //MD->isMoveAssignmentOperator()
        //MD->isOverloadedOperator();
        //MD->isStaticOverloadedOperator();

        //
        // CXXDestructorDecl
        //
        // if constexpr(std::derived_from<DeclTy, CXXDestructorDecl>)
        // {
        // }

        //
        // CXXConstructorDecl
        //
        if constexpr(std::derived_from<DeclTy, CXXConstructorDecl>)
        {
            populate(I.Explicit, D->getExplicitSpecifier());
        }

        //
        // CXXConversionDecl
        //
        if constexpr(std::derived_from<DeclTy, CXXConversionDecl>)
        {
            populate(I.Explicit, D->getExplicitSpecifier());
        }
    }

    ArrayRef<ParmVarDecl*> const params = FD->parameters();
    I.Params.resize(params.size());
    for (std::size_t i = 0; i < params.size(); ++i)
    {
        ParmVarDecl const* P = params[i];
        MRDOCS_SYMBOL_TRACE(P, context_);
        Param& param = I.Params[i];

        if (param.Name.empty())
        {
            param.Name = P->getName();
        }

        if (!param.Type)
        {
            param.Type = toTypeInfo(P->getOriginalType());
        }

        const Expr* default_arg = P->hasUninstantiatedDefaultArg() ?
            P->getUninstantiatedDefaultArg() : P->getInit();
        if (param.Default.empty() &&
            default_arg)
        {
            param.Default = getSourceCode(default_arg->getSourceRange());
            param.Default = trim(param.Default);
            if (param.Default.starts_with("= "))
            {
                param.Default.erase(0, 2);
                param.Default = ltrim(param.Default);
            }
        }
    }

    I.Class = toFunctionClass(FD->getDeclKind());

    // extract the return type in direct dependency mode
    // if it contains a placeholder type which is
    // deduceded as a local class type
    QualType const RT = FD->getReturnType();
    MRDOCS_SYMBOL_TRACE(RT, context_);
    I.ReturnType = toTypeInfo(RT);

    if (auto* TRC = FD->getTrailingRequiresClause())
    {
        populate(I.Requires, TRC);
    }

    populateAttributes(I, D);
}

void
ASTVisitor::
populate(FunctionInfo& I, FunctionTemplateDecl* D)
{
    FunctionDecl* TD = D->getTemplatedDecl();
    populate(I.Template, TD, D);
    populate(I, TD);
}

void
ASTVisitor::
populate(
    EnumInfo& I,
    EnumDecl* D)
{
    I.Scoped = D->isScoped();
    if (D->isFixed())
    {
        I.UnderlyingType = toTypeInfo(D->getIntegerType());
    }
}

void
ASTVisitor::
populate(
    EnumConstantInfo& I,
    EnumConstantDecl* D)
{
    I.Name = extractName(D);
    populate(
        I.Initializer,
        D->getInitExpr(),
        D->getInitVal());
}

template<std::derived_from<TypedefNameDecl> TypedefNameDeclTy>
void
ASTVisitor::
populate(
    TypedefInfo& I,
    TypedefNameDeclTy* D)
{
    I.IsUsing = isa<TypeAliasDecl>(D);
    QualType const QT = D->getUnderlyingType();
    I.Type = toTypeInfo(QT);
}

void
ASTVisitor::
populate(TypedefInfo& I, TypeAliasTemplateDecl* D)
{
    populate(I.Template, D->getTemplatedDecl(), D);
    populate(I, D->getTemplatedDecl());
}


void
ASTVisitor::
populate(
    VariableInfo& I,
    VarDecl* D)
{
    // KRYSTIAN FIXME: we need to properly merge storage class
    if (StorageClass const SC = D->getStorageClass())
    {
        I.StorageClass = toStorageClassKind(SC);
    }
    // this handles thread_local, as well as the C
    // __thread and __Thread_local specifiers
    I.IsThreadLocal |= D->getTSCSpec() !=
        ThreadStorageClassSpecifier::TSCS_unspecified;
    // KRYSTIAN NOTE: VarDecl does not provide getConstexprKind,
    // nor does it use getConstexprKind to store whether
    // a variable is constexpr/constinit. Although
    // only one is permitted in a variable declaration,
    // it is possible to declare a static data member
    // as both constexpr and constinit in separate declarations..
    I.IsConstinit |= D->hasAttr<ConstInitAttr>();
    I.IsConstexpr |= D->isConstexpr();
    I.IsInline |= D->isInline();
    if (Expr const* E = D->getInit())
    {
        populate(I.Initializer, E);
    }
    auto QT = D->getType();
    if (D->isConstexpr()) {
        // when D->isConstexpr() is true, QT contains a redundant
        // `const` qualifier which we need to remove
        QT.removeLocalConst();
    }
    I.Type = toTypeInfo(QT);
    populateAttributes(I, D);
}

void
ASTVisitor::
populate(
    FieldInfo& I,
    FieldDecl* D)
{
    I.Type = toTypeInfo(D->getType());
    I.IsVariant = D->getParent()->isUnion();
    I.IsMutable = D->isMutable();
    if (Expr const* E = D->getInClassInitializer())
    {
        populate(I.Default, E);
    }
    if(D->isBitField())
    {
        I.IsBitfield = true;
        populate(I.BitfieldWidth, D->getBitWidth());
    }
    I.HasNoUniqueAddress = D->hasAttr<NoUniqueAddressAttr>();
    I.IsDeprecated = D->hasAttr<DeprecatedAttr>();
    I.IsMaybeUnused = D->hasAttr<UnusedAttr>();
    populateAttributes(I, D);
}

void
ASTVisitor::
populate(VariableInfo& I, VarTemplateDecl* D)
{
    populate(I.Template, D->getTemplatedDecl(), D);
    populate(I, D->getTemplatedDecl());
}

void
ASTVisitor::
populate(VariableInfo& I, VarTemplateSpecializationDecl* D)
{
    populate(I.Template, D, D->getSpecializedTemplate());
    populate(I, cast<VarDecl>(D));
}

void
ASTVisitor::
populate(
    SpecializationInfo& I,
    ClassTemplateSpecializationDecl* D)
{
    CXXRecordDecl const* PD = getInstantiatedFrom(D);
    populate(I.Args, D->getTemplateArgs().asArray());
    generateID(PD, I.Primary);
}

void
ASTVisitor::
populate(
    FriendInfo& I,
    FriendDecl* D)
{
    // A NamedDecl nominated by a FriendDecl
    // will be one of the following:
    // - FunctionDecl
    // - FunctionTemplateDecl
    // - ClassTemplateDecl
    if (NamedDecl* ND = D->getFriendDecl())
    {
        generateID(ND, I.FriendSymbol);
        // If this is a friend function declaration naming
        // a previously undeclared function, traverse it.
        // in addition to this, traverse the declaration if
        // it's a class templates first declared as a friend
        if((ND->isFunctionOrFunctionTemplate() &&
            ND->getFriendObjectKind() == Decl::FOK_Undeclared) ||
            (isa<ClassTemplateDecl>(ND) && ND->isFirstDecl()))
        {
            traverse(cast<Decl>(ND));
        }
    }
    // Since a friend declaration which name non-class types
    // will be ignored, a type nominated by a FriendDecl can
    // essentially be anything
    if (TypeSourceInfo const* TSI = D->getFriendType())
    {
        I.FriendType = toTypeInfo(TSI->getType());
    }
}

void
ASTVisitor::
populate(
    GuideInfo& I,
    CXXDeductionGuideDecl* D)
{
    I.Deduced = toTypeInfo(D->getReturnType());
    for (const ParmVarDecl* P : D->parameters())
    {
        I.Params.emplace_back(
            toTypeInfo(P->getOriginalType()),
            P->getNameAsString(),
            // deduction guides cannot have default arguments
            std::string());
    }
    populate(I.Explicit, D->getExplicitSpecifier());
}

void
ASTVisitor::
populate(
    GuideInfo& I,
    FunctionTemplateDecl* D)
{
    populate(I.Template, D->getTemplatedDecl(), D);
    populate(I, cast<CXXDeductionGuideDecl>(D->getTemplatedDecl()));
}

void
ASTVisitor::
populate(
    NamespaceAliasInfo& I,
    NamespaceAliasDecl* D)
{
    NamedDecl const* Aliased = D->getAliasedNamespace();
    NestedNameSpecifier const* NNS = D->getQualifier();
    I.AliasedSymbol = toNameInfo(Aliased, {}, NNS);
}


void
ASTVisitor::
populate(
    UsingInfo& I,
    UsingDecl* D)
{
    I.Class = UsingClass::Normal;
    I.Qualifier = toNameInfo(D->getQualifier());
    for (UsingShadowDecl const* UDS: D->shadows())
    {
        ScopeExitRestore s(mode_, ExtractionMode::Dependency);
        Decl* S = UDS->getTargetDecl();
        Info* SI = findOrTraverse(S);
        if (SI)
        {
            I.UsingSymbols.emplace_back(SI->id);
        }
    }
}

void
ASTVisitor::
populate(
    ConceptInfo& I,
    ConceptDecl* D)
{
    populate(I.Template, D, D);
    populate(I.Constraint, D->getConstraintExpr());
}


/*  Default function to populate template parameters
 */
template <
    std::derived_from<Decl> DeclTy,
    std::derived_from<TemplateDecl> TemplateDeclTy>
void
ASTVisitor::
populate(TemplateInfo& Template, DeclTy*, TemplateDeclTy* TD)
{
    MRDOCS_ASSERT(TD);
    TemplateParameterList const* TPL = TD->getTemplateParameters();
    populate(Template, TPL);
}

template<std::derived_from<CXXRecordDecl> CXXRecordDeclTy>
void
ASTVisitor::
populate(
    TemplateInfo& Template,
    CXXRecordDeclTy*,
    ClassTemplateDecl* CTD)
{
    MRDOCS_ASSERT(CTD);
    populate(Template, CTD->getTemplateParameters());
}

void
ASTVisitor::
populate(
    TemplateInfo& Template,
    ClassTemplateSpecializationDecl* CTSD,
    ClassTemplateDecl* CTD)
{
    MRDOCS_ASSERT(CTD);

    // If D is a partial/explicit specialization, extract the template arguments
    generateID(getInstantiatedFrom(CTD), Template.Primary);

    // Extract the template arguments of the specialization
    if (const auto* argsAsWritten = CTSD->getTemplateArgsAsWritten()) {
        populate(Template.Args, argsAsWritten);
    } else {
        // Implicit specializations do not have template arguments as written
        populate(Template.Args, CTSD->getTemplateArgs().asArray());
    }

    // Extract the template parameters if this is a partial specialization
    if (auto* CTPSD = dyn_cast<ClassTemplatePartialSpecializationDecl>(CTSD))
    {
        TemplateParameterList* params = CTPSD->getTemplateParameters();
        populate(Template, params);
    }

}

template<std::derived_from<VarDecl> VarDeclTy>
void
ASTVisitor::
populate(
    TemplateInfo& Template,
    VarDeclTy* D,
    VarTemplateDecl* VTD)
{
    MRDOCS_ASSERT(VTD);

    // If D is a partial/explicit specialization, extract the template arguments
    if(auto* VTSD = dyn_cast<VarTemplateSpecializationDecl>(D))
    {
        generateID(getInstantiatedFrom(VTD), Template.Primary);
        // extract the template arguments of the specialization
        populate(Template.Args, VTSD->getTemplateArgsAsWritten());
        // extract the template parameters if this is a partial specialization
        if(auto* VTPSD = dyn_cast<VarTemplatePartialSpecializationDecl>(D))
            populate(Template, VTPSD->getTemplateParameters());
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
    const FunctionProtoType* FPT)
{
    MRDOCS_ASSERT(FPT);
    I.Implicit = ! FPT->hasNoexceptExceptionSpec();
    I.Kind = toNoexceptKind(
        FPT->getExceptionSpecType());
    // store the operand, if any
    if (Expr const* NoexceptExpr = FPT->getNoexceptExpr())
    {
        I.Operand = toString(NoexceptExpr);
    }
}

void
ASTVisitor::
populate(
    ExplicitInfo& I,
    const ExplicitSpecifier& ES)
{
    I.Implicit = ! ES.isSpecified();
    I.Kind = toExplicitKind(ES);

    // store the operand, if any
    if (Expr const* ExplicitExpr = ES.getExpr())
    {
        I.Operand = toString(ExplicitExpr);
    }
}

void
ASTVisitor::
populate(
    ExprInfo& I,
    const Expr* E)
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
    const Expr* E)
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
    const Expr* E,
    const llvm::APInt& V)
{
    populate(I, E);
    I.Value.emplace(toInteger<T>(V));
}

template
void
ASTVisitor::
populate<std::uint64_t>(
    ConstantExprInfo<std::uint64_t>& I,
    const Expr* E,
    const llvm::APInt& V);

void
ASTVisitor::
populate(
    std::unique_ptr<TParam>& I,
    const NamedDecl* N)
{
    visit(N, [&]<typename DeclTy>(const DeclTy* P)
    {
        constexpr Decl::Kind kind =
            DeclToKind<DeclTy>();

        if constexpr(kind == Decl::TemplateTypeParm)
        {
            if (!I)
            {
                I = std::make_unique<TypeTParam>();
            }
            auto* R = dynamic_cast<TypeTParam*>(I.get());
            if (P->wasDeclaredWithTypename())
            {
                R->KeyKind = TParamKeyKind::Typename;
            }
            if (P->hasDefaultArgument() && !R->Default)
            {
                R->Default = toTArg(
                    P->getDefaultArgument().getArgument());
            }
            if(const TypeConstraint* TC = P->getTypeConstraint())
            {
                const NestedNameSpecifier* NNS =
                    TC->getNestedNameSpecifierLoc().getNestedNameSpecifier();
                std::optional<const ASTTemplateArgumentListInfo*> TArgs;
                if (TC->hasExplicitTemplateArgs())
                {
                    TArgs.emplace(TC->getTemplateArgsAsWritten());
                }
                R->Constraint = toNameInfo(TC->getNamedConcept(), TArgs, NNS);
            }
            return;
        }
        else if constexpr(kind == Decl::NonTypeTemplateParm)
        {
            if (!I)
            {
                I = std::make_unique<NonTypeTParam>();
            }
            auto* R = dynamic_cast<NonTypeTParam*>(I.get());
            R->Type = toTypeInfo(P->getType());
            if (P->hasDefaultArgument() && !R->Default)
            {
                R->Default = toTArg(
                    P->getDefaultArgument().getArgument());
            }
            return;
        }
        else if constexpr(kind == Decl::TemplateTemplateParm)
        {
            if (!I)
            {
                I = std::make_unique<TemplateTParam>();
            }
            TemplateTemplateParmDecl const* TTPD = cast<TemplateTemplateParmDecl>(P);
            MRDOCS_CHECK_OR(TTPD);
            TemplateParameterList const* TPL = TTPD->getTemplateParameters();
            MRDOCS_CHECK_OR(TPL);
            auto* Result = dynamic_cast<TemplateTParam*>(I.get());
            if (Result->Params.size() < TPL->size())
            {
                Result->Params.resize(TPL->size());
            }
            for (std::size_t i = 0; i < TPL->size(); ++i)
            {
                NamedDecl const* TP = TPL->getParam(i);
                populate(Result->Params[i], TP);
            }
            if (TTPD->hasDefaultArgument() && !Result->Default)
            {
                TemplateArgumentLoc const& TAL = TTPD->getDefaultArgument();
                TemplateArgument const& TA = TAL.getArgument();
                Result->Default = toTArg(TA);
            }
            return;
        }
        MRDOCS_UNREACHABLE();
    });

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
    TemplateParameterList const* TPL)
{
    if (!TPL)
    {
        return;
    }
    if (TPL->size() > TI.Params.size())
    {
        TI.Params.resize(TPL->size());
    }
    for (std::size_t i = 0; i < TPL->size(); ++i)
    {
        NamedDecl const* P = TPL->getParam(i);
        populate(TI.Params[i], P);
    }
    if (auto* RC = TPL->getRequiresClause())
    {
        populate(TI.Requires, RC);
    }
}

void
ASTVisitor::
populate(
    std::vector<std::unique_ptr<TArg>>& result,
    const ASTTemplateArgumentListInfo* args)
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

template <std::derived_from<Info> InfoTy>
void
ASTVisitor::
populateAttributes(InfoTy& I, const Decl* D)
{
    if constexpr (requires { I.Attributes; })
    {
        MRDOCS_CHECK_OR(D->hasAttrs());
        for (Attr const* attr: D->getAttrs())
        {
            IdentifierInfo const* II = attr->getAttrName();
            if (!II)
            {
                continue;
            }
            if (std::ranges::find(I.Attributes, II->getName()) == I.Attributes.end())
            {
                I.Attributes.emplace_back(II->getName());
            }
        }
    }
}

std::string
ASTVisitor::
extractName(NamedDecl const* D)
{
    return extractName(D->getDeclName());
}

std::string
ASTVisitor::
extractName(DeclarationName const N)
{
    std::string result;
    if (N.isEmpty())
    {
        return result;
    }
    switch(N.getNameKind())
    {
    case DeclarationName::Identifier:
        if (auto const* I = N.getAsIdentifierInfo())
        {
            result.append(I->getName());
        }
        break;
    case DeclarationName::CXXDestructorName:
        result.push_back('~');
        [[fallthrough]];
    case DeclarationName::CXXConstructorName:
        if (auto const* R = N.getCXXNameType()->getAsCXXRecordDecl())
        {
            result.append(R->getIdentifier()->getName());
        }
        break;
    case DeclarationName::CXXDeductionGuideName:
        if (auto const* T = N.getCXXDeductionGuideTemplate())
        {
            result.append(T->getIdentifier()->getName());
        }
        break;
    case DeclarationName::CXXConversionFunctionName:
    {
        result.append("operator ");
        // KRYSTIAN FIXME: we *really* should not be
        // converting types to strings like this
        result.append(mrdocs::toString(*toTypeInfo(N.getCXXNameType())));
        break;
    }
    case DeclarationName::CXXOperatorName:
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
    case DeclarationName::CXXLiteralOperatorName:
    case DeclarationName::CXXUsingDirective:
        break;
    default:
        MRDOCS_UNREACHABLE();
    }
    return result;
}

bool
ASTVisitor::
generateJavadoc(
    std::unique_ptr<Javadoc>& javadoc,
    Decl const* D)
{
    RawComment const* RC =
        D->getASTContext().getRawCommentForDeclNoCache(D);
    MRDOCS_CHECK_OR(RC, false);
    comments::FullComment* FC =
        RC->parse(D->getASTContext(), &sema_.getPreprocessor(), D);
    MRDOCS_CHECK_OR(FC, false);
    parseJavadoc(javadoc, FC, D, config_, diags_);
    return true;
}

std::unique_ptr<TypeInfo>
ASTVisitor::
toTypeInfo(QualType const qt)
{
    MRDOCS_SYMBOL_TRACE(qt, context_);

    // The qualified symbol referenced by a regular symbol is a dependency.
    // For library types, can be proved wrong and the Info type promoted
    // to a regular type later on if the type matches the regular
    // extraction criteria
    ScopeExitRestore s(mode_, ExtractionMode::Dependency);

    // Build the TypeInfo representation for the type
    TypeInfoBuilder Builder(*this);
    Builder.Visit(qt);
    return Builder.result();
}

std::unique_ptr<NameInfo>
ASTVisitor::
toNameInfo(
    NestedNameSpecifier const* NNS)
{
    if (!NNS)
    {
        return nullptr;
    }
    MRDOCS_SYMBOL_TRACE(NNS, context_);

    ScopeExitRestore scope(mode_,ExtractionMode::Dependency);
    std::unique_ptr<NameInfo> I = nullptr;
    if (const Type* T = NNS->getAsType())
    {
        NameInfoBuilder Builder(*this, NNS->getPrefix());
        Builder.Visit(T);
        I = Builder.result();
    }
    else if(const IdentifierInfo* II = NNS->getAsIdentifier())
    {
        I = std::make_unique<NameInfo>();
        I->Name = II->getName();
        I->Prefix = toNameInfo(NNS->getPrefix());
    }
    else if(const NamespaceDecl* ND = NNS->getAsNamespace())
    {
        I = std::make_unique<NameInfo>();
        I->Name = ND->getIdentifier()->getName();
        I->Prefix = toNameInfo(NNS->getPrefix());
        Decl const* ID = getInstantiatedFrom(ND);
        if (Info* info = findOrTraverse(const_cast<Decl*>(ID)))
        {
            I->id = info->id;
        }
    }
    else if(const NamespaceAliasDecl* NAD = NNS->getAsNamespaceAlias())
    {
        I = std::make_unique<NameInfo>();
        I->Name = NAD->getIdentifier()->getName();
        I->Prefix = toNameInfo(NNS->getPrefix());
        Decl const* ID = getInstantiatedFrom(NAD);
        if (Info* info = findOrTraverse(const_cast<Decl*>(ID)))
        {
            I->id = info->id;
        }
    }
    return I;
}

template <class TArgRange>
std::unique_ptr<NameInfo>
ASTVisitor::
toNameInfo(
    DeclarationName const Name,
    std::optional<TArgRange> TArgs,
    NestedNameSpecifier const* NNS)
{
    if (Name.isEmpty())
    {
        return nullptr;
    }
    std::unique_ptr<NameInfo> I = nullptr;
    if(TArgs)
    {
        auto Specialization = std::make_unique<SpecializationNameInfo>();
        populate(Specialization->TemplateArgs, *TArgs);
        I = std::move(Specialization);
    }
    else
    {
        I = std::make_unique<NameInfo>();
    }
    I->Name = extractName(Name);
    if (NNS)
    {
        I->Prefix = toNameInfo(NNS);
    }
    return I;
}

template <class TArgRange>
std::unique_ptr<NameInfo>
ASTVisitor::
toNameInfo(
    Decl const* D,
    std::optional<TArgRange> TArgs,
    NestedNameSpecifier const* NNS)
{
    const auto* ND = dyn_cast_if_present<NamedDecl>(D);
    if (!ND)
    {
        return nullptr;
    }
    auto I = toNameInfo(
        ND->getDeclName(), std::move(TArgs), NNS);
    if (!I)
    {
        return nullptr;
    }
    ScopeExitRestore scope(mode_, ExtractionMode::Dependency);
    auto* ID = getInstantiatedFrom(D);
    if (Info const* info = findOrTraverse(const_cast<Decl*>(ID)))
    {
        I->id = info->id;
    }
    return I;
}

template
std::unique_ptr<NameInfo>
ASTVisitor::
toNameInfo<llvm::ArrayRef<clang::TemplateArgument>>(
    Decl const* D,
    std::optional<llvm::ArrayRef<clang::TemplateArgument>> TArgs,
    NestedNameSpecifier const* NNS);

std::unique_ptr<TArg>
ASTVisitor::
toTArg(const TemplateArgument& A)
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
    case TemplateArgument::Null:
        break;

    // a template argument pack (any kind)
    case TemplateArgument::Pack:
    {
        // we should never a TemplateArgument::Pack here
        MRDOCS_UNREACHABLE();
        break;
    }
    // type
    case TemplateArgument::Type:
    {
        auto R = std::make_unique<TypeTArg>();
        QualType QT = A.getAsType();
        MRDOCS_ASSERT(! QT.isNull());
        // if the template argument is a pack expansion,
        // use the expansion pattern as the type & mark
        // the template argument as a pack expansion
        if(const Type* T = QT.getTypePtr();
            auto* PT = dyn_cast<PackExpansionType>(T))
        {
            R->IsPackExpansion = true;
            QT = PT->getPattern();
        }
        R->Type = toTypeInfo(QT);
        return R;
    }
    // pack expansion of a template name
    case TemplateArgument::TemplateExpansion:
    // template name
    case TemplateArgument::Template:
    {
        auto R = std::make_unique<TemplateTArg>();
        R->IsPackExpansion = A.isPackExpansion();

        // KRYSTIAN FIXME: template template arguments are
        // id-expression, so we don't properly support them yet.
        // for the time being, we will use the name & SymbolID of
        // the referenced declaration (if it isn't dependent),
        // and fallback to printing the template name otherwise
        TemplateName const TN = A.getAsTemplateOrTemplatePattern();
        if(auto* TD = TN.getAsTemplateDecl())
        {
            if(auto* II = TD->getIdentifier())
                R->Name = II->getName();
            // do not extract a SymbolID or build Info if
            // the template template parameter names a
            // template template parameter or builtin template
            if(! isa<TemplateTemplateParmDecl>(TD) &&
                ! isa<BuiltinTemplateDecl>(TD))
            {
                // upsertDependency(getInstantiatedFrom<
                //    NamedDecl>(TD), R->Template);
            }
        }
        else
        {
            llvm::raw_string_ostream stream(R->Name);
            TN.print(stream, context_.getPrintingPolicy(),
                TemplateName::Qualified::AsWritten);
        }
        return R;
    }
    // nullptr value
    case TemplateArgument::NullPtr:
    // expression referencing a declaration
    case TemplateArgument::Declaration:
    // integral expression
    case TemplateArgument::Integral:
    // expression
    case TemplateArgument::Expression:
    {
        auto R = std::make_unique<NonTypeTArg>();
        R->IsPackExpansion = A.isPackExpansion();
        // if this is a pack expansion, use the template argument
        // expansion pattern in place of the template argument pack
        const TemplateArgument& adjusted =
            R->IsPackExpansion ?
            A.getPackExpansionPattern() : A;

        llvm::raw_string_ostream stream(R->Value.Written);
        adjusted.print(context_.getPrintingPolicy(), stream, false);

        return R;
    }
    default:
        MRDOCS_UNREACHABLE();
    }
    return nullptr;
}


std::string
ASTVisitor::
toString(const Expr* E)
{
    std::string result;
    llvm::raw_string_ostream stream(result);
    E->printPretty(stream, nullptr, context_.getPrintingPolicy());
    return result;
}

std::string
ASTVisitor::
toString(const Type* T)
{
    if(auto* AT = dyn_cast_if_present<AutoType>(T))
    {
        switch(AT->getKeyword())
        {
        case AutoTypeKeyword::Auto:
        case AutoTypeKeyword::GNUAutoType:
            return "auto";
        case AutoTypeKeyword::DecltypeAuto:
            return "decltype(auto)";
        default:
            MRDOCS_UNREACHABLE();
        }
    }
    if(auto* TTPT = dyn_cast_if_present<TemplateTypeParmType>(T))
    {
        if (TemplateTypeParmDecl* TTPD = TTPT->getDecl();
            TTPD && TTPD->isImplicit())
        {
            return "auto";
        }
    }
    return QualType(T, 0).getAsString(
        context_.getPrintingPolicy());
}

template<class Integer>
Integer
ASTVisitor::
toInteger(const llvm::APInt& V)
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
getSourceCode(SourceRange const& R) const
{
    return Lexer::getSourceText(
        CharSourceRange::getTokenRange(R),
        source_,
        context_.getLangOpts()).str();
}

std::optional<ASTVisitor::SFINAEInfo>
ASTVisitor::
extractSFINAEInfo(QualType const T)
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
    auto const param_arg = tryGetTemplateArgument(
        SFINAEControl->Parameters, Args, SFINAEControl->ParamIdx);
    MRDOCS_CHECK_OR(param_arg, std::nullopt);
    MRDOCS_SYMBOL_TRACE(*param_arg, context_);

    // Create a vector of template arguments that represent the
    // controlling parameters of the SFINAE template
    std::vector<TemplateArgument> ControllingArgs;
    for (std::size_t I = 0; I < Args.size(); ++I)
    {
        if (SFINAEControl->ControllingParams[I])
        {
            MRDOCS_SYMBOL_TRACE(Args[I], context_);
            ControllingArgs.emplace_back(Args[I]);
        }
    }

    // Return the main type and controlling types
    return SFINAEInfo{param_arg->getAsType(), std::move(ControllingArgs)};
}

std::optional<ASTVisitor::SFINAEControlParams>
ASTVisitor::
getSFINAEControlParams(
    TemplateDecl* TD,
    IdentifierInfo const* Member)
{
    MRDOCS_SYMBOL_TRACE(TD, context_);
    MRDOCS_SYMBOL_TRACE(Member, context_);
    MRDOCS_CHECK_OR(TD, std::nullopt);

    // The `FindParam` lambda function is used to find the index of a
    // template argument in a list of template arguments. It is used
    // to find the index of the controlling parameter in the list of
    // template arguments of the template declaration.
    auto FindParam = [this](
        ArrayRef<TemplateArgument> Arguments,
        const TemplateArgument& Arg) -> std::size_t
    {
        if (Arg.getKind() != TemplateArgument::Type)
        {
            return -1;
        }
        auto const It = std::ranges::find_if(
            Arguments,
            [&](const TemplateArgument& Other)
            {
                if (Other.getKind() != TemplateArgument::Type)
                {
                    return false;
                }
                return context_.hasSameType(Other.getAsType(), Arg.getAsType());
            });
        bool const found = It != Arguments.end();
        return found ? It - Arguments.data() : static_cast<std::size_t>(-1);
    };

    if(auto* ATD = dyn_cast<TypeAliasTemplateDecl>(TD))
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
        auto param_arg = tryGetTemplateArgument(
            sfinaeControl->Parameters,
            underlyingTemplateInfo->Arguments,
            sfinaeControl->ParamIdx);
        MRDOCS_CHECK_OR(param_arg, std::nullopt);
        MRDOCS_SYMBOL_TRACE(*param_arg, context_);

        // Find the index of the parameter that represents the SFINAE result
        // in the primary template arguments
        unsigned ParamIdx = FindParam(ATD->getInjectedTemplateArgs(context_), *param_arg);

        // Return the controlling parameters with values corresponding to
        // the primary template arguments
        TemplateParameterList* primaryTemplParams = ATD->getTemplateParameters();
        MRDOCS_SYMBOL_TRACE(primaryTemplParams, context_);
        return SFINAEControlParams(
            primaryTemplParams,
            std::move(sfinaeControl->ControllingParams),
            ParamIdx);
    }

    // Ensure this is a ClassTemplateDecl
    auto* CTD = dyn_cast<ClassTemplateDecl>(TD);
    MRDOCS_SYMBOL_TRACE(CTD, context_);
    MRDOCS_CHECK_OR(CTD, std::nullopt);

    // Get the template arguments of the primary template
    auto PrimaryArgs = CTD->getInjectedTemplateArgs(context_);
    MRDOCS_SYMBOL_TRACE(PrimaryArgs, context_);

    // Type of the member that represents the SFINAE result.
    QualType MemberType;

    // Index of the parameter that represents the the SFINAE result.
    // For instance, in the specialization `std::enable_if<true,T>::type`,
    // `type` is `T`, which corresponds to the second template parameter
    // `T`, so `ParamIdx` is `1` to represent the second parameter.
    unsigned ParamIdx = -1;

    // The `IsMismatch` function checks if there's a mismatch between the
    // CXXRecordDecl of the ClassTemplateDecl and the specified template
    // arguments. If there's a mismatch and `IsMismatch` returns `true`,
    // the caller returns `std::nullopt` to indicate that the template
    // is not a SFINAE template. If there are no mismatches, the caller
    // continues to check the controlling parameters of the template.
    // This function also updates the `MemberType` and `ParamIdx` variables
    // so that they can be used to check the controlling parameters.
    auto IsMismatch = [&](CXXRecordDecl* RD, ArrayRef<TemplateArgument> Args)
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
        QualType CurrentType;
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
                auto param_arg = tryGetTemplateArgument(
                    template_params, sfinae_info->Arguments, param_idx);
                if(! param_arg)
                    return true;
                auto CurrentTypeFromBase = param_arg->getAsType();
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
            if (auto* TND = dyn_cast<TypedefNameDecl>(MemberLookup.front()))
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
            TemplateArgument asTemplateArg(CurrentType);
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
            TemplateArgument MappedPrimary = PrimaryArgs[FoundIdx];
            MRDOCS_SYMBOL_TRACE(MappedPrimary, context_);
            // The primary argument in SFINAE should be a type
            MRDOCS_ASSERT(MappedPrimary.getKind() == TemplateArgument::Type);
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
    CXXRecordDecl* PrimaryRD = CTD->getTemplatedDecl();
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
        ArrayRef<TemplateArgument> SpecArgs = CTSD->getTemplateArgs().asArray();
        MRDOCS_CHECK_OR(!IsMismatch(CTSD, SpecArgs), std::nullopt);
    }

    // Check if there's a mismatch between any partial specialization and the arguments
    SmallVector<ClassTemplatePartialSpecializationDecl*> PartialSpecs;
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
            TemplateArgument Arg = PartialArgs[i];
            MRDOCS_SYMBOL_TRACE(Arg, context_);
            switch (Arg.getKind())
            {
            case TemplateArgument::Integral:
            case TemplateArgument::Declaration:
            case TemplateArgument::StructuralValue:
            case TemplateArgument::NullPtr:
                break;
            case TemplateArgument::Expression:
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

std::optional<ASTVisitor::SFINAETemplateInfo>
ASTVisitor::getSFINAETemplateInfo(QualType T, bool const AllowDependentNames) const
{
    MRDOCS_SYMBOL_TRACE(T, context_);
    MRDOCS_ASSERT(!T.isNull());

    // If the type is an elaborated type, get the named type
    if (auto* ET = T->getAs<ElaboratedType>())
    {
        T = ET->getNamedType();
    }

    // If the type is a dependent name type and dependent names are allowed,
    // extract the identifier and the qualifier's type
    SFINAETemplateInfo SFINAE;
    if (auto* DNT = T->getAsAdjusted<DependentNameType>();
        DNT && AllowDependentNames)
    {
        SFINAE.Member = DNT->getIdentifier();
        MRDOCS_SYMBOL_TRACE(SFINAE.Member, context_);
        T = QualType(DNT->getQualifier()->getAsType(), 0);
        MRDOCS_SYMBOL_TRACE(T, context_);
    }

    // If the type is a template specialization type, extract the template name
    // and the template arguments
    if (auto* TST = T->getAsAdjusted<TemplateSpecializationType>())
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

std::optional<TemplateArgument>
ASTVisitor::
tryGetTemplateArgument(
    TemplateParameterList* Parameters,
    ArrayRef<TemplateArgument> const Arguments,
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
    NamedDecl* ND = Parameters->getParam(Index);
    MRDOCS_SYMBOL_TRACE(ND, context_);
    if(auto* TTPD = dyn_cast<TemplateTypeParmDecl>(ND);
        TTPD && TTPD->hasDefaultArgument())
    {
        MRDOCS_SYMBOL_TRACE(TTPD, context_);
        return TTPD->getDefaultArgument().getArgument();
    }
    if(auto* NTTPD = dyn_cast<NonTypeTemplateParmDecl>(ND);
        NTTPD && NTTPD->hasDefaultArgument())
    {
        MRDOCS_SYMBOL_TRACE(NTTPD, context_);
        return NTTPD->getDefaultArgument().getArgument();
    }
    return std::nullopt;
}

bool
ASTVisitor::
shouldExtract(
    const Decl* D,
    AccessSpecifier const access)
{
    // The translation unit is always extracted as the
    // global namespace. It can't fail any of the filters
    // because its qualified name is represented by the
    // empty string, and it has no file associated with it.
    MRDOCS_CHECK_OR(!isa<TranslationUnitDecl>(D), true);

    // Check if this kind of symbol should be extracted.
    // This filters symbols supported by mrdocs and
    // symbol types whitelisted in the configuration,
    // such as private members and anonymous namespaces.
    MRDOCS_CHECK_OR(checkTypeFilters(D, access), false);

    // In dependency mode, we don't need the file and symbol
    // filters because this is a dependency of another
    // declaration that passes the filters.
    if (mode_ == ExtractionMode::Dependency)
    {
        // If the whole declaration is implicit, we should
        // not promote the extraction mode to regular
        // even if it passes the filters. We should
        // extract `D` in dependency mode so that
        // its symbol ID is available but there's
        // no need to extract its members.
        if (isAllImplicit(D))
        {
            return true;
        }

        // So, the filters are used to determine if we
        // should upgrade the extraction mode already.
        // This is not a scoped promotion because
        // parents and members should also assume
        // the same base extraction mode.
        if (checkSymbolFilters(D) &&
            checkFileFilters(D))
        {
            mode_ = ExtractionMode::Regular;
        }
        // But we return true either way
        return true;
    }

    // Check if this symbol should be extracted according
    // to its qualified name. This checks if it matches
    // the symbol patterns and if it's not excluded.
    MRDOCS_CHECK_OR(checkSymbolFilters(D), false);

    // Check if this symbol should be extracted according
    // to its location. This checks if it's in one of the
    // input directories, if it matches the file patterns,
    // and it's not in an excluded file.
    MRDOCS_CHECK_OR(checkFileFilters(D), false);

    return true;
}

bool
ASTVisitor::
checkTypeFilters(Decl const* D, AccessSpecifier access)
{
    if (!config_->privateMembers)
    {
        // KRYSTIAN FIXME: this doesn't handle direct
        // dependencies on inaccessible declarations
        MRDOCS_CHECK_OR(
             access != AccessSpecifier::AS_private, false);
    }

    // Don't extract anonymous unions
    const auto* RD = dyn_cast<RecordDecl>(D);
    MRDOCS_CHECK_OR(!RD || !RD->isAnonymousStructOrUnion(), false);

    // Don't extract implicitly generated declarations
    // (except for IndirectFieldDecls)
    MRDOCS_CHECK_OR(!D->isImplicit() || isa<IndirectFieldDecl>(D), false);

    // Don't extract anonymous namespaces unless configured to do so
    // and the current mode is normal
    if (const auto* ND = dyn_cast<NamespaceDecl>(D);
        ND &&
        ND->isAnonymousNamespace() &&
        config_->anonymousNamespaces)
    {
        // Otherwise, skip extraction if this isn't a dependency
        // KRYSTIAN FIXME: is this correct? a namespace should not
        // be extracted as a dependency (until namespace aliases and
        // using directives are supported)
        MRDOCS_CHECK_OR(mode_ == ExtractionMode::Regular, false);
    }

    return true;
}

bool
ASTVisitor::
checkFileFilters(Decl const* D)
{
    clang::SourceLocation Loc = D->getBeginLoc();
    if (Loc.isInvalid())
    {
        Loc = D->getLocation();
    }
    FileInfo* fileInfo = findFileInfo(Loc);
    MRDOCS_CHECK_OR(fileInfo, false);

    // Check pre-processed file filters
    MRDOCS_CHECK_OR(!fileInfo->passesFilters, *fileInfo->passesFilters);

    // Call the non-cached version of this function
    bool const ok = checkFileFilters(fileInfo->full_path);
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

bool
ASTVisitor::
checkSymbolFilters(Decl const* D) const
{
    // If not a NamedDecl, then symbol filters don't apply
    const auto* ND = dyn_cast<NamedDecl>(D);
    MRDOCS_CHECK_OR(ND, true);
    return checkSymbolFilters(ND, isa<DeclContext>(D));
}

bool
ASTVisitor::
checkSymbolFilters(NamedDecl const* ND, bool const isScope) const
{
    SmallString<256> const name = qualifiedName(ND);
    return checkSymbolFilters(name.str(), isScope);
}

bool
ASTVisitor::
checkSymbolFilters(std::string_view const symbolName, bool const isScope) const
{
    if (isScope)
    {
        return checkSymbolFiltersImpl<true>(symbolName);
    }
    return checkSymbolFiltersImpl<false>(symbolName);
}

template <bool isScope>
bool
ASTVisitor::
checkSymbolFiltersImpl(std::string_view const symbolName) const
{
    // Don't extract declarations that fail the symbol filter
    auto includeMatchFn = [&](SymbolGlobPattern const& pattern)
    {
        if constexpr (isScope)
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
        else
        {
            return pattern.match(symbolName);
        }
    };
    MRDOCS_CHECK_OR(
        config_->includeSymbols.empty() ||
        std::ranges::any_of(config_->includeSymbols, includeMatchFn), false);

    // Don't extract declarations that fail the exclude symbol filter
    auto excludeMatchFn = [&](SymbolGlobPattern const& pattern)
    {
        // Unlike the include filter, we want to match the entire symbol name
        // for the exclude filter regardless of whether the symbol is a scope.
        // If the scope is explicitly excluded, we already know we want to
        // exclude all symbols in that scope
        return pattern.match(symbolName);
    };
    MRDOCS_CHECK_OR(
        config_->excludeSymbols.empty() ||
        std::ranges::none_of(config_->excludeSymbols, excludeMatchFn), false);

    return true;
}

SmallString<256>
ASTVisitor::
qualifiedName(Decl const* D) const
{
    if (auto* ND = dyn_cast<NamedDecl>(D))
    {
        return qualifiedName(ND);
    }
    return {};
}

SmallString<256>
ASTVisitor::
qualifiedName(NamedDecl const* ND) const
{
    SmallString<256> name;
    llvm::raw_svector_ostream stream(name);
    getQualifiedName(ND, stream, context_.getPrintingPolicy());
    return name;
}

Info*
ASTVisitor::
find(SymbolID const& id)
{
    if (auto const it = info_.find(id); it != info_.end())
    {
        return it->get();
    }
    return nullptr;
}

Info*
ASTVisitor::
find(Decl* D)
{
    auto ID = generateID(D);
    MRDOCS_CHECK_OR(ID, nullptr);
    return find(ID);
}


// KRYSTIAN NOTE: we *really* should not have a
// type named "SourceLocation"...
ASTVisitor::FileInfo*
ASTVisitor::
findFileInfo(clang::SourceLocation const loc)
{
    MRDOCS_CHECK_OR(!loc.isInvalid(), nullptr);

    // KRYSTIAN FIXME: we really should not be calling getPresumedLoc this often,
    // it's quite expensive
    auto const presumed = source_.getPresumedLoc(loc, false);
    MRDOCS_CHECK_OR(!presumed.isInvalid(), nullptr);

    const FileEntry* entry = source_.getFileEntryForID( presumed.getFileID());
    MRDOCS_CHECK_OR(entry, nullptr);

    // Find in the cache
    if (auto const it = files_.find(entry); it != files_.end())
    {
        return std::addressof(it->second);
    }

    // Build FileInfo
    auto const FI = buildFileInfo(entry);
    MRDOCS_CHECK_OR(FI, nullptr);
    auto [it, inserted] = files_.try_emplace(entry, std::move(*FI));
    return std::addressof(it->second);
}

std::optional<ASTVisitor::FileInfo>
ASTVisitor::
buildFileInfo(FileEntry const* entry)
{
    std::string_view const file_path = entry->tryGetRealPathName();
    MRDOCS_CHECK_OR(!file_path.empty(), std::nullopt);
    return buildFileInfo(file_path);
}

ASTVisitor::FileInfo
ASTVisitor::
buildFileInfo(std::string_view const file_path)
{
    FileInfo file_info;
    file_info.full_path = file_path;
    if (!files::isPosixStyle(file_info.full_path))
    {
        file_info.full_path = files::makePosixStyle(file_info.full_path);
    }

    // Attempts to get a relative path for the prefix
    auto tryGetRelativePosixPath = [&file_info](std::string_view const prefix)
        -> std::optional<std::string_view>
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
        -> std::optional<std::string_view>
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
    for (HeaderSearch& HS = sema_.getPreprocessor().getHeaderSearchInfo();
         DirectoryLookup const& DL : HS.search_dir_range())
    {
        OptionalDirectoryEntryRef DR = DL.getDirRef();
        if (!DL.isNormalDir() || !DR)
        {
            // Only consider normal directories
            continue;
        }
        StringRef searchDir = DR->getName();
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
    std::optional<std::string> const optEnvPathsStr = llvm::sys::Process::GetEnv("PATH");
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

template <std::derived_from<Info> InfoTy>
ASTVisitor::upsertResult<InfoTy>
ASTVisitor::
upsert(SymbolID const& id)
{
    // Creating symbol with invalid SymbolID
    MRDOCS_ASSERT(id != SymbolID::invalid);
    Info* info = find(id);
    bool const isNew = !info;
    if (!info)
    {
        info = info_.emplace(std::make_unique<
            InfoTy>(id)).first->get();
        info->Extraction = mostSpecific(info->Extraction, mode_);
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
upsert(DeclType* D)
{
    AccessSpecifier access = getAccess(D);
    if (!shouldExtract(D, access))
    {
        return Unexpected(Error("Symbol should not be extracted"));
    }

    SymbolID const id = generateID(D);
    MRDOCS_CHECK_MSG(id, "Failed to extract symbol ID");

    using R = std::conditional_t<
        std::same_as<InfoTy, void>,
        InfoTypeFor_t<DeclType>,
        InfoTy>;
    auto [I, isNew] = upsert<R>(id);
    I.Access = toAccessKind(access);

    // If the symbol was previously extracted as a dependency
    // and is now being extracted as a regular symbol because
    // it passed the more constrained filters, update the
    // extraction mode and set the symbol as new so it's populated
    // this time.
    bool const previouslyExtractedAsDependency =
        !isNew &&
        mode_ != ExtractionMode::Dependency &&
        I.Extraction == ExtractionMode::Dependency;
    if (previouslyExtractedAsDependency)
    {
        I.Extraction = mostSpecific(I.Extraction, mode_);
        isNew = true;
    }

    return upsertResult<R>{std::ref(I), isNew};
}

} // clang::mrdocs
