//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "lib/AST/ClangHelpers.hpp"
#include <mrdocs/Support/Assert.hpp>
#include <clang/Sema/Template.h>
#include <clang/Index/USRGeneration.h>

namespace clang::mrdocs {

const Expr*
SubstituteConstraintExpressionWithoutSatisfaction(
    Sema &S, const Sema::TemplateCompareNewDeclInfo &DeclInfo,
    const Expr *ConstrExpr)
{
    MultiLevelTemplateArgumentList MLTAL = S.getTemplateInstantiationArgs(
        DeclInfo.getDecl(), DeclInfo.getLexicalDeclContext(), /*Final=*/false,
        /*Innermost=*/std::nullopt,
        /*RelativeToPrimary=*/true,
        /*Pattern=*/nullptr, /*ForConstraintInstantiation=*/true,
        /*SkipForSpecialization*/ false);

    if (MLTAL.getNumSubstitutedLevels() == 0)
    {
        return ConstrExpr;
    }

    Sema::SFINAETrap const SFINAE(S, /*AccessCheckingSFINAE=*/false);

    Sema::InstantiatingTemplate Inst(
        S, DeclInfo.getLocation(),
        Sema::InstantiatingTemplate::ConstraintNormalization{},
        const_cast<NamedDecl *>(DeclInfo.getDecl()), SourceRange{});
    if (Inst.isInvalid())
    {
        return nullptr;
    }

    // Set up a dummy 'instantiation' scope in the case of reference to function
    // parameters that the surrounding function hasn't been instantiated yet. Note
    // this may happen while we're comparing two templates' constraint
    // equivalence.
    LocalInstantiationScope ScopeForParameters(S);
    if (auto *FD = DeclInfo.getDecl()->getAsFunction())
    {
        for (auto *PVD : FD->parameters())
        {
            ScopeForParameters.InstantiatedLocal(PVD, PVD);
        }
    }

    std::optional<Sema::CXXThisScopeRAII> ThisScope;

    // See TreeTransform::RebuildTemplateSpecializationType. A context scope is
    // essential for having an injected class as the canonical type for a template
    // specialization type at the rebuilding stage. This guarantees that, for
    // out-of-line definitions, injected class name types and their equivalent
    // template specializations can be profiled to the same value, which makes it
    // possible that e.g. constraints involving C<Class<T>> and C<Class> are
    // perceived identical.
    std::optional<Sema::ContextRAII> ContextScope;
    if (auto *RD = dyn_cast<CXXRecordDecl>(DeclInfo.getDeclContext()))
    {
        ThisScope.emplace(S, const_cast<CXXRecordDecl *>(RD), Qualifiers());
        ContextScope.emplace(S, const_cast<DeclContext *>(cast<DeclContext>(RD)),
                             /*NewThisContext=*/false);
    }
    ExprResult SubstConstr = S.SubstConstraintExprWithoutSatisfaction(
        const_cast<clang::Expr *>(ConstrExpr), MLTAL);
    if (SFINAE.hasErrorOccurred() || !SubstConstr.isUsable())
    {
        return nullptr;
    }
    return SubstConstr.get();
}

AccessSpecifier
getAccess(const Decl* D)
{
    // First, get the declaration this was instantiated from
    D = getInstantiatedFrom(D);

    // If this is the template declaration of a template,
    // use the access of the template
    if (TemplateDecl const* TD = D->getDescribedTemplate())
    {
        return TD->getAccessUnsafe();
    }

    // For class/variable template partial/explicit specializations,
    // we want to use the access of the primary template
    if (auto const* CTSD = dyn_cast<ClassTemplateSpecializationDecl>(D))
    {
        return CTSD->getSpecializedTemplate()->getAccessUnsafe();
    }

    if (auto const* VTSD = dyn_cast<VarTemplateSpecializationDecl>(D))
    {
        return VTSD->getSpecializedTemplate()->getAccessUnsafe();
    }

    // For function template specializations, use the access of the
    // primary template if it has been resolved
    if(auto const* FD = dyn_cast<FunctionDecl>(D))
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
    if(auto const* FD = dyn_cast<FriendDecl>(D))
    {
        auto const* RD = dyn_cast<CXXRecordDecl>(
            FD->getLexicalDeclContext());
        // RD should never be null in well-formed code,
        // but clang error recovery may build an AST
        // where the assumption will not hold
        if (!RD)
        {
            return AccessSpecifier::AS_public;
        }
        auto access = RD->isClass() ?
            AccessSpecifier::AS_private :
            AccessSpecifier::AS_public;
        for(auto* M : RD->decls())
        {
            if (auto* AD = dyn_cast<AccessSpecDecl>(M))
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

QualType
getDeclaratorType(const DeclaratorDecl* DD)
{
    if (auto* TSI = DD->getTypeSourceInfo();
        TSI && !TSI->getType().isNull())
    {
        return TSI->getType();
    }
    return DD->getType();
}

NonTypeTemplateParmDecl const*
getNTTPFromExpr(const Expr* E, unsigned const Depth)
{
    while(true)
    {
        if(const auto* ICE = dyn_cast<ImplicitCastExpr>(E))
        {
            E = ICE->getSubExpr();
            continue;
        }
        if(const auto* CE = dyn_cast<ConstantExpr>(E))
        {
            E = CE->getSubExpr();
            continue;
        }
        if(const auto* SNTTPE = dyn_cast<SubstNonTypeTemplateParmExpr>(E))
        {
            E = SNTTPE->getReplacement();
            continue;
        }
        if(const auto* CCE = dyn_cast<CXXConstructExpr>(E);
            CCE && CCE->getParenOrBraceRange().isInvalid())
        {
            // look through implicit copy construction from an lvalue of the same type
            E = CCE->getArg(0);
            continue;
        }
        break;
    }

    const auto* DRE = dyn_cast<DeclRefExpr>(E);
    if (!DRE)
    {
        return nullptr;
    }

    const auto* NTTPD = dyn_cast<NonTypeTemplateParmDecl>(DRE->getDecl());
    if (!NTTPD || NTTPD->getDepth() != Depth)
    {
        return nullptr;
    }

    return NTTPD;
}

Decl*
getParentDecl(Decl* D)
{
    while((D = cast_if_present<
        Decl>(D->getDeclContext())))
    {
        switch(D->getKind())
        {
        case Decl::CXXRecord:
            // we treat anonymous unions as "transparent"
            if (auto const* RD = cast<CXXRecordDecl>(D);
                RD->isAnonymousStructOrUnion())
            {
                break;
            }
            [[fallthrough]];
        case Decl::TranslationUnit:
        case Decl::Namespace:
        case Decl::Enum:
        case Decl::ClassTemplateSpecialization:
        case Decl::ClassTemplatePartialSpecialization:
            return D;
        // we consider all other DeclContexts to be "transparent"
        default:
            break;
        }
    }
    return nullptr;
}

} // clang::mrdocs
