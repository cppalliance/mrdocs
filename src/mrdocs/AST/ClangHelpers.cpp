//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "ClangHelpers.hpp"
#include <mrdocs/Support/Error/Assert.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <mrdocs/Support/Report.hpp>
#include <clang/AST/RawCommentList.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Driver/Driver.h>
#include <clang/Sema/EnterExpressionEvaluationContext.h>
#include <clang/Sema/Template.h>
#include <clang/UnifiedSymbolResolution/USRGeneration.h>
#include <llvm/Option/ArgList.h>
#include <ranges>

namespace mrdocs {

// Ported from Clang's file-local static `SubstituteConstraintExpressionWithoutSatisfaction`
// in clang/lib/Sema/SemaConcept.cpp. No public header exposes it, and the public
// `Sema::AreConstraintExpressionsEqual` returns only a bool, not the substituted
// expression MrDocs needs, so we vendor a copy. Synced to the pinned revision
// 77e43ec1; re-diff against that file on each LLVM bump.
clang::Expr const*
SubstituteConstraintExpressionWithoutSatisfaction(
    clang::Sema &S, const clang::Sema::TemplateCompareNewDeclInfo &DeclInfo,
    clang::Expr const* ConstrExpr)
{
    clang::MultiLevelTemplateArgumentList MLTAL = S.getTemplateInstantiationArgs(
        DeclInfo.getDecl(), DeclInfo.getDeclContext(), /*Final=*/false,
        /*Innermost=*/std::nullopt,
        /*RelativeToPrimary=*/true,
        /*Pattern=*/nullptr, /*ForConstraintInstantiation=*/true,
        /*SkipForSpecialization*/ false);

    if (MLTAL.getNumSubstitutedLevels() == 0)
    {
        return ConstrExpr;
    }

    // Clang's own `AreConstraintExpressionsEqual` wraps the calls to this
    // helper in a `SFINAETrap`. MrDocs invokes the helper directly, so the trap
    // lives here instead: it traps substitution errors and supplies the SFINAE
    // context that `SubstConstraintExprWithoutSatisfaction` expects on the
    // instantiation stack.
    clang::Sema::SFINAETrap const SFINAE(S, /*AccessCheckingSFINAE=*/false);

    // Set up a dummy 'instantiation' scope in the case of reference to function
    // parameters that the surrounding function hasn't been instantiated yet. Note
    // this may happen while we're comparing two templates' constraint
    // equivalence.
    std::optional<clang::LocalInstantiationScope> ScopeForParameters;
    if (clang::NamedDecl const* ND = DeclInfo.getDecl();
        ND && ND->isFunctionOrFunctionTemplate())
    {
        ScopeForParameters.emplace(S, /*CombineWithOuterScope=*/true);
        clang::FunctionDecl const* FD = ND->getAsFunction();
        if (clang::FunctionTemplateDecl* Template =
                FD->getDescribedFunctionTemplate();
            Template && Template->getInstantiatedFromMemberTemplate())
        {
            FD = Template->getInstantiatedFromMemberTemplate()->getTemplatedDecl();
        }
        for (auto* PVD : FD->parameters())
        {
            if (ScopeForParameters->getInstantiationOfIfExists(PVD))
            {
                continue;
            }
            if (!PVD->isParameterPack())
            {
                ScopeForParameters->InstantiatedLocal(PVD, PVD);
                continue;
            }
            // Map the parameter pack to a size-of-1 argument so its canonical
            // type is used when comparing redeclarations for equivalence.
            ScopeForParameters->MakeInstantiatedLocalArgPack(PVD);
            ScopeForParameters->InstantiatedLocalPackArg(PVD, PVD);
        }
    }

    std::optional<clang::Sema::CXXThisScopeRAII> ThisScope;

    // See TreeTransform::RebuildTemplateSpecializationType. A context scope is
    // essential for having an injected class as the canonical type for a template
    // specialization type at the rebuilding stage. This guarantees that, for
    // out-of-line definitions, injected class name types and their equivalent
    // template specializations can be profiled to the same value, which makes it
    // possible that e.g. constraints involving C<Class<T>> and C<Class> are
    // perceived identical.
    std::optional<clang::Sema::ContextRAII> ContextScope;
    clang::DeclContext const* DC = [&] {
        if (!DeclInfo.getDecl())
        {
            return DeclInfo.getDeclContext();
        }
        return DeclInfo.getDecl()->getFriendObjectKind()
                   ? DeclInfo.getLexicalDeclContext()
                   : DeclInfo.getDeclContext();
    }();
    if (auto* RD = dyn_cast<clang::CXXRecordDecl>(DC))
    {
        ThisScope.emplace(
            S, const_cast<clang::CXXRecordDecl*>(RD), clang::Qualifiers());
        ContextScope.emplace(
            S, const_cast<clang::DeclContext*>(cast<clang::DeclContext>(RD)),
            /*NewThisContext=*/false);
    }
    clang::EnterExpressionEvaluationContext UnevaluatedContext(
        S, clang::Sema::ExpressionEvaluationContext::Unevaluated,
        clang::Sema::ReuseLambdaContextDecl);
    clang::ExprResult SubstConstr = S.SubstConstraintExprWithoutSatisfaction(
        const_cast<clang::Expr *>(ConstrExpr), MLTAL);
    if (SFINAE.hasErrorOccurred() || !SubstConstr.isUsable())
    {
        return nullptr;
    }
    return SubstConstr.get();
}

clang::Decl const*
canonicalFriendTarget(clang::NamedDecl const* ND)
{
    if (!ND)
        return nullptr;

    if (auto const* CTSD = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(ND))
    {
        if (auto* T = CTSD->getSpecializedTemplate())
            return T->getTemplatedDecl()->getCanonicalDecl();
    }

    if (auto const* CRD = llvm::dyn_cast<clang::CXXRecordDecl>(ND))
    {
        if (auto* CTD = CRD->getDescribedClassTemplate())
            return CTD->getTemplatedDecl()->getCanonicalDecl();
    }

    if (auto const* VTS = llvm::dyn_cast<clang::VarTemplateSpecializationDecl>(ND))
    {
        if (auto* VT = VTS->getSpecializedTemplate())
            return VT->getTemplatedDecl()->getCanonicalDecl();
    }

    if (auto const* VTD = llvm::dyn_cast<clang::VarTemplateDecl>(ND))
        return VTD->getTemplatedDecl()->getCanonicalDecl();

    if (auto const* FD = llvm::dyn_cast<clang::FunctionDecl>(ND))
    {
        if (auto const* PT = FD->getPrimaryTemplate())
            return PT->getTemplatedDecl()->getCanonicalDecl();
    }

    if (auto const* FTD = llvm::dyn_cast<clang::FunctionTemplateDecl>(ND))
        return FTD->getTemplatedDecl()->getCanonicalDecl();

    return ND->getCanonicalDecl();
}

clang::AccessSpecifier
getAccess(clang::Decl const* D)
{
    // First, get the declaration this was instantiated from
    D = getInstantiatedFrom(D);

    // If this is the template declaration of a template,
    // use the access of the template
    if (clang::TemplateDecl const* TD = D->getDescribedTemplate())
    {
        return TD->getAccessUnsafe();
    }

    // For class/variable template partial/explicit specializations,
    // we want to use the access of the primary template
    if (auto const* CTSD = dyn_cast<clang::ClassTemplateSpecializationDecl>(D))
    {
        return CTSD->getSpecializedTemplate()->getAccessUnsafe();
    }

    if (auto const* VTSD = dyn_cast<clang::VarTemplateSpecializationDecl>(D))
    {
        return VTSD->getSpecializedTemplate()->getAccessUnsafe();
    }

    // For function template specializations, use the access of the
    // primary template if it has been resolved
    if(auto const* FD = dyn_cast<clang::FunctionDecl>(D))
    {
        if (auto const* FTD = FD->getPrimaryTemplate())
        {
            return FTD->getAccessUnsafe();
        }
    }

    // Since friend declarations are not members, this hack computes
    // their access based on the default access for the tag they
    // appear in, and any AccessSpecDecls which appears lexically
    // before them
    if(auto const* FD = dyn_cast<clang::FriendDecl>(D))
    {
        auto const* RD = dyn_cast<clang::CXXRecordDecl>(
            FD->getLexicalDeclContext());
        // RD should never be null in well-formed code,
        // but clang error recovery may build an AST
        // where the assumption will not hold
        if (!RD)
        {
            return clang::AccessSpecifier::AS_public;
        }
        auto access = RD->isClass() ?
            clang::AccessSpecifier::AS_private :
            clang::AccessSpecifier::AS_public;
        for(auto* M : RD->decls())
        {
            if (auto* AD = dyn_cast<clang::AccessSpecDecl>(M))
            {
                access = AD->getAccessUnsafe();
            } else if (M == FD)
            {
                return access;
            }
        }
        // KRYSTIAN FIXME: will this ever be hit?
        // it would require a friend declaration that is
        // not in the lexical traversal of its lexical context
        MRDOCS_UNREACHABLE();
    }

    // In all other cases, use the access of this declaration
    return D->getAccessUnsafe();
}

clang::QualType
getDeclaratorType(clang::DeclaratorDecl const* DD)
{
    if (auto* TSI = DD->getTypeSourceInfo();
        TSI && !TSI->getType().isNull())
    {
        return TSI->getType();
    }
    return DD->getType();
}

clang::NonTypeTemplateParmDecl const*
getNTTPFromExpr(clang::Expr const* E, unsigned const Depth)
{
    while(true)
    {
        if(auto const* ICE = dyn_cast<clang::ImplicitCastExpr>(E))
        {
            E = ICE->getSubExpr();
            continue;
        }
        if(auto const* CE = dyn_cast<clang::ConstantExpr>(E))
        {
            E = CE->getSubExpr();
            continue;
        }
        if(auto const* SNTTPE = dyn_cast<clang::SubstNonTypeTemplateParmExpr>(E))
        {
            E = SNTTPE->getReplacement();
            continue;
        }
        if(auto const* CCE = dyn_cast<clang::CXXConstructExpr>(E);
            CCE && CCE->getParenOrBraceRange().isInvalid())
        {
            // look through implicit copy construction from an lvalue of the same type
            E = CCE->getArg(0);
            continue;
        }
        break;
    }

    auto const* DRE = dyn_cast<clang::DeclRefExpr>(E);
    if (!DRE)
    {
        return nullptr;
    }

    auto const* NTTPD = dyn_cast<clang::NonTypeTemplateParmDecl>(DRE->getDecl());
    if (!NTTPD || NTTPD->getDepth() != Depth)
    {
        return nullptr;
    }

    return NTTPD;
}

clang::Decl const*
getParent(clang::Decl const* D)
{
    while((D = cast_if_present<clang::Decl>(D->getDeclContext())))
    {
        switch(D->getKind())
        {
        case clang::Decl::CXXRecord:
            // we treat anonymous unions as "transparent"
            if (auto const* RD = cast<clang::CXXRecordDecl>(D);
                RD &&
                RD->isAnonymousStructOrUnion())
            {
                break;
            }
            [[fallthrough]];
        case clang::Decl::TranslationUnit:
        case clang::Decl::Namespace:
        case clang::Decl::Enum:
        case clang::Decl::ClassTemplateSpecialization:
        case clang::Decl::ClassTemplatePartialSpecialization:
            // we treat anonymous namespaces as "transparent"
            if (auto const* ND = dyn_cast<clang::NamespaceDecl>(D);
                ND &&
                (ND->isInlineNamespace() ||
                 ND->isAnonymousNamespace()))
            {
                break;
            }
            return D;
        // we consider all other DeclContexts to be "transparent"
        default:
            break;
        }
    }
    return nullptr;
}

void
getQualifiedName(
    clang::NamedDecl const* ND,
    clang::raw_ostream& stream,
    clang::PrintingPolicy const& policy)
{
    if (auto const* CTS = dyn_cast<clang::ClassTemplateSpecializationDecl>(ND))
    {
        CTS->getSpecializedTemplate()->printQualifiedName(stream, policy);
        clang::TemplateArgumentList const& args = CTS->getTemplateArgs();
        stream << '<';
        for (unsigned i = 0, e = args.size(); i != e; ++i) {
            if (args[i].getIsDefaulted())
            {
                break;
            }
            if (i)
            {
                stream << ",";
            }
            args[i].print(policy, stream, true);
        }
        stream << '>';
    }
    else
    {
        ND->printQualifiedName(stream, policy);
    }
}

clang::Decl const*
decayToPrimaryTemplate(clang::Decl const* D)
{
#ifndef NDEBUG
    // Print only the class header (name and template args if specialization)
    llvm::SmallString<128> symbolName;
    if (const auto* ND = dyn_cast<clang::NamedDecl>(D))
    {
        if (const auto* CTS = dyn_cast<clang::ClassTemplateSpecializationDecl>(ND)) {
            llvm::raw_svector_ostream os(symbolName);
            CTS->getSpecializedTemplate()->printQualifiedName(os, CTS->getASTContext().getPrintingPolicy());
            const clang::TemplateArgumentList& args = CTS->getTemplateArgs();
            os << '<';
            for (unsigned i = 0, e = args.size(); i != e; ++i)
            {
                if (i) os << ", ";
                args[i].print(CTS->getASTContext().getPrintingPolicy(), os, true);
            }
            os << '>';
        } else if (ND) {
            llvm::raw_svector_ostream os(symbolName);
            ND->printQualifiedName(os, ND->getASTContext().getPrintingPolicy());
        }
    }
    else
    {
        llvm::raw_svector_ostream os(symbolName);
        os << "<unnamed " << D->clang::Decl::getDeclKindName() << ">";
    }
    llvm::raw_svector_ostream os(symbolName);
    report::trace("symbolName: ", std::string_view(os.str()));
#endif

    clang::Decl const* ID = D;

    // Check parent
    if (clang::CXXRecordDecl const* ClassParent = dyn_cast<clang::CXXRecordDecl>(getParent(ID)))
    {
        if (clang::Decl const* DecayedClassParent = decayToPrimaryTemplate(ClassParent);
            DecayedClassParent != ClassParent &&
            isa<clang::ClassTemplateSpecializationDecl>(DecayedClassParent))
        {
            auto const* RD = dyn_cast<clang::ClassTemplateDecl>(DecayedClassParent);
            clang::CXXRecordDecl* RDParent = RD->getTemplatedDecl();
            auto* NamedID = dyn_cast<clang::NamedDecl>(ID);
            auto NamedDecls = RDParent->decls()
                | std::ranges::views::transform([](clang::Decl* C) { return dyn_cast<clang::NamedDecl>(C); })
                | std::ranges::views::filter([](clang::NamedDecl* C) { return C; });
            for (clang::NamedDecl const* Child : NamedDecls)
            {
                if (Child->getDeclName() == NamedID->getDeclName() &&
                    Child->getKind() == ID->getKind())
                {
                    ID = Child;
                    break;
                }
            }
        }
    }

    // Check template specialization
    if (auto const* TSD = dynamic_cast<clang::ClassTemplateSpecializationDecl const*>(ID);
        TSD &&
        !TSD->isExplicitSpecialization())
    {
        ID = TSD->getSpecializedTemplate();
    }

    return ID;
}

bool
isAllImplicitSpecialization(clang::Decl const* D)
{
    if (!D)
    {
        return true;
    }
    if (auto const* TSD = dynamic_cast<clang::ClassTemplateSpecializationDecl const*>(D);
        TSD &&
        TSD->isExplicitSpecialization())
    {
        return false;
    }
    if (auto const* TSD = dynamic_cast<clang::VarTemplateSpecializationDecl const*>(D);
        TSD &&
        TSD->isExplicitSpecialization())
    {
        return false;
    }
    auto const* P = getParent(D);
    return isAllImplicitSpecialization(P);
}

bool
isAnyImplicitSpecialization(clang::Decl const* D)
{
    if (!D)
    {
        return false;
    }
    if (auto const* TSD = dynamic_cast<clang::ClassTemplateSpecializationDecl const*>(D);
        TSD &&
        !TSD->isExplicitSpecialization())
    {
        return true;
    }
    if (auto const* TSD = dynamic_cast<clang::VarTemplateSpecializationDecl const*>(D);
        TSD &&
        !TSD->isExplicitSpecialization())
    {
        return true;
    }
    auto const* P = getParent(D);
    return isAnyImplicitSpecialization(P);
}

bool
isInstantiation(clang::Decl const* D)
{
    // The kinds an instantiation written in a context can have: `template`
    // gives a definition, `extern template` a declaration. An explicit
    // specialization is a declaration of the user's own and is not one.
    auto const written = [](clang::TemplateSpecializationKind const TSK)
    {
        return TSK == clang::TSK_ExplicitInstantiationDeclaration ||
               TSK == clang::TSK_ExplicitInstantiationDefinition;
    };
    if (auto const* CTSD = dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(D))
    {
        return written(CTSD->getSpecializationKind());
    }
    if (auto const* VTSD = dyn_cast_or_null<clang::VarTemplateSpecializationDecl>(D))
    {
        return written(VTSD->getSpecializationKind());
    }
    if (auto const* FD = dyn_cast_or_null<clang::FunctionDecl>(D))
    {
        // A function template's instantiation has TemplateSpecializationInfo.
        // A member function of an instantiated class has the same
        // specialization kind but MemberSpecializationInfo instead: it is a
        // member, not an instantiation written in the context.
        return FD->getTemplateSpecializationInfo() != nullptr &&
               written(FD->getTemplateSpecializationKind());
    }
    return false;
}

bool
isAnySpecialization(clang::Decl const* D)
{
    if (!D)
    {
        return false;
    }
    if (isa<clang::ClassTemplateSpecializationDecl>(D))
    {
        return true;
    }
    if (isa<clang::VarTemplateSpecializationDecl>(D))
    {
        return true;
    }
    if (auto const* FD = dyn_cast<clang::FunctionDecl>(D);
        FD && FD->isFunctionTemplateSpecialization())
    {
        return true;
    }
    auto const* P = getParent(D);
    return isAnySpecialization(P);
}

bool
isVirtualMember(clang::Decl const* D)
{
    if (auto const* MD = dyn_cast<clang::CXXMethodDecl>(D))
    {
        return MD->isVirtual();
    }
    return false;
}

bool
isAnonymousNamespace(clang::Decl const* D)
{
    if (auto const* ND = dyn_cast<clang::NamespaceDecl>(D))
    {
        return ND->isAnonymousNamespace();
    }
    return false;
}

bool
isStaticFileLevelMember(clang::Decl const* D)
{
    if (const auto *VD = dyn_cast<clang::VarDecl>(D)) {
        return VD->getStorageClass() == clang::SC_Static && VD->getDeclContext()->isFileContext();
    }
    if (const auto *FD = dyn_cast<clang::FunctionDecl>(D)) {
        return FD->getStorageClass() == clang::SC_Static && FD->getDeclContext()->isFileContext();
    }
    return false;
}

bool
isImplicitDefaultInit(clang::Expr const* E)
{
    clang::CXXConstructExpr const* ctor =
        dyn_cast<clang::CXXConstructExpr>(E->IgnoreImplicit());
    return ctor != nullptr
        && ctor->getNumArgs() == 0
        && ctor->getParenOrBraceRange().isInvalid();
}

// Mirror the situations in which Clang's file-local getLocsForCommentSearch
// (clang/lib/AST/ASTContext.cpp) returns no search location, so
// getRawCommentNoCache never even looks for an attached comment. These are the
// cases where our begin-location retry can recover a comment that the standard
// lookup could not. Ported against the pinned revision 77e43ec1; re-diff
// against that function on each LLVM bump.
static bool
hasNoCommentSearchLoc(clang::Decl const* D)
{
    // Implicit declarations and implicit template instantiations carry no
    // user-attachable comment location of their own.
    if (D->isImplicit())
    {
        return true;
    }
    if (auto const* FD = dyn_cast<clang::FunctionDecl>(D))
    {
        if (FD->getTemplateSpecializationKind() == clang::TSK_ImplicitInstantiation)
        {
            return true;
        }
    }
    if (auto const* VD = dyn_cast<clang::VarDecl>(D))
    {
        if (VD->isStaticDataMember() &&
            VD->getTemplateSpecializationKind() == clang::TSK_ImplicitInstantiation)
        {
            return true;
        }
    }
    if (auto const* CRD = dyn_cast<clang::CXXRecordDecl>(D))
    {
        if (CRD->getTemplateSpecializationKind() == clang::TSK_ImplicitInstantiation)
        {
            return true;
        }
    }
    if (auto const* CTSD = dyn_cast<clang::ClassTemplateSpecializationDecl>(D))
    {
        clang::TemplateSpecializationKind const TSK = CTSD->getSpecializationKind();
        if (TSK == clang::TSK_ImplicitInstantiation || TSK == clang::TSK_Undeclared)
        {
            return true;
        }
    }
    if (auto const* ED = dyn_cast<clang::EnumDecl>(D))
    {
        if (ED->getTemplateSpecializationKind() == clang::TSK_ImplicitInstantiation)
        {
            return true;
        }
    }
    // A tag declaration (not definition) embedded in another declaration's
    // decl-specifier-seq, e.g. a `friend class Z;`.
    if (auto const* TD = dyn_cast<clang::TagDecl>(D))
    {
        if (TD->isEmbeddedInDeclarator() && !TD->isCompleteDefinition())
        {
            return true;
        }
    }
    // Parameters and template parameters do not get their own comment lookup.
    return isa<clang::ParmVarDecl>(D) ||
           isa<clang::TemplateTypeParmDecl>(D) ||
           isa<clang::NonTypeTemplateParmDecl>(D) ||
           isa<clang::TemplateTemplateParmDecl>(D);
}

clang::RawComment const*
getDocumentation(clang::Decl const* D)
{
    clang::ASTContext const& ctx = D->getASTContext();
    clang::RawComment const* RC = ctx.getRawCommentNoCache(D);

    if (!RC)
    {
        // getRawCommentNoCache misses an attached comment in a few situations
        // that this retry, anchored at the declaration's begin location, can
        // recover. Gate on them so the retry stays off the hot path for the
        // common undocumented symbol:
        //
        //   1. A braced return type. getRawCommentNoCache anchors at the
        //      declaration's name and rejects the comment when the text up to
        //      the name contains any of `;{}#@`; a leading return type that
        //      spells one of those (e.g. `decltype(int{})` or a dependent
        //      `reference_t<Q{}, U{}>`) trips it. Anchoring before the return
        //      type avoids the offending text.
        //      See tests/golden/fixtures/symbols/function/doc-comment-braces-in-return-type.cpp
        //
        //   2. A declaration for which getLocsForCommentSearch returns no
        //      location, so no comment is looked up at all: a tag embedded in a
        //      declarator (a `friend class Z;`) or an implicit template
        //      instantiation borrowing its primary's comment.
        //      See tests/golden/fixtures/symbols/record/friend-type.cpp
        clang::SourceManager const& sm = ctx.getSourceManager();
        clang::SourceLocation const beginLoc =
            sm.getExpansionLoc(D->getBeginLoc());
        clang::SourceLocation const nameLoc =
            sm.getExpansionLoc(D->getLocation());

        bool retry = hasNoCommentSearchLoc(D);
        if (!retry &&
            beginLoc.isValid() && beginLoc.isFileID() && nameLoc.isFileID())
        {
            auto const [beginFile, beginOffset] = sm.getDecomposedLoc(beginLoc);
            auto const [nameFile, nameOffset] = sm.getDecomposedLoc(nameLoc);
            bool invalid = false;
            llvm::StringRef const buffer = sm.getBufferData(beginFile, &invalid);
            retry = !invalid &&
                    beginFile == nameFile &&
                    beginOffset < nameOffset &&
                    buffer.substr(beginOffset, nameOffset - beginOffset)
                        .find_first_of(";{}#@") != llvm::StringRef::npos;
        }

        if (retry && beginLoc.isValid() && beginLoc.isFileID())
        {
            clang::FileID const fid = sm.getDecomposedLoc(beginLoc).first;
            if (auto const* commentsInFile = ctx.Comments.getCommentsInFile(fid))
            {
                RC = ctx.getRawCommentNoCacheImpl(D, beginLoc, *commentsInFile);
            }
        }
    }

    if (!RC)
    {
        if (auto const* TD = dyn_cast<clang::TemplateDecl>(D))
        {
            if (clang::NamedDecl const* ND = TD->getTemplatedDecl())
            {
                RC = ctx.getRawCommentNoCache(ND);
            }
        }
    }

    // A nested-namespace-definition, `namespace a::b {`, opens one
    // NamespaceDecl per component at the same place, so Clang hands the
    // comment above it to every one of them. The comment describes what
    // is being opened for the code that follows, the innermost namespace,
    // not the enclosing ones that merely lead to it. Give it to the outer
    // components' redeclarations elsewhere if they have their own.
    // See tests/golden/fixtures/symbols/namespace/nested-namespace-definition-doc.cpp
    if (RC)
    {
        if (auto const* NS = dyn_cast<clang::NamespaceDecl>(D))
        {
            for (clang::Decl const* Child : NS->decls())
            {
                auto const* Inner = dyn_cast<clang::NamespaceDecl>(Child);
                if (Inner && Inner->isNested() &&
                    ctx.getRawCommentNoCache(Inner) == RC)
                {
                    return nullptr;
                }
                break;
            }
        }
    }
    return RC;
}

bool
isDocumented(clang::Decl const* D)
{
    return getDocumentation(D) != nullptr;
}

bool
isClangCL(clang::tooling::CompileCommand const& cc)
{
    auto const& cmdline = cc.CommandLine;

    // ------------------------------------------------------
    // Convert to InputArgList
    // ------------------------------------------------------
    // InputArgList is the input format for llvm functions
    auto cmdLineCStrsView = std::views::transform(cmdline, &std::string::c_str);
    std::vector const
        cmdLineCStrs(cmdLineCStrsView.begin(), cmdLineCStrsView.end());
    llvm::opt::InputArgList const
        args(cmdLineCStrs.data(), cmdLineCStrs.data() + cmdLineCStrs.size());

    // ------------------------------------------------------
    // Get driver mode
    // ------------------------------------------------------
    // The driver mode distinguishes between clang/gcc and msvc
    // command line option formats. The value is deduced from
    // the `-drive-mode` option or from `progName`.
    // Common values are "gcc", "g++", "cpp", "cl" and "flang".
    std::string const& progName = cmdline.front();
    clang::StringRef const driver_mode = clang::driver::
        getDriverMode(progName, cmdLineCStrs);

    return clang::driver::IsClangCL(driver_mode);
}

} // mrdocs
