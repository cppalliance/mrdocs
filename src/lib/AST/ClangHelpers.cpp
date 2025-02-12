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
#include <ranges>

namespace clang::mrdocs {

Expr const*
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
getAccess(Decl const* D)
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
getDeclaratorType(DeclaratorDecl const* DD)
{
    if (auto* TSI = DD->getTypeSourceInfo();
        TSI && !TSI->getType().isNull())
    {
        return TSI->getType();
    }
    return DD->getType();
}

NonTypeTemplateParmDecl const*
getNTTPFromExpr(Expr const* E, unsigned const Depth)
{
    while(true)
    {
        if(auto const* ICE = dyn_cast<ImplicitCastExpr>(E))
        {
            E = ICE->getSubExpr();
            continue;
        }
        if(auto const* CE = dyn_cast<ConstantExpr>(E))
        {
            E = CE->getSubExpr();
            continue;
        }
        if(auto const* SNTTPE = dyn_cast<SubstNonTypeTemplateParmExpr>(E))
        {
            E = SNTTPE->getReplacement();
            continue;
        }
        if(auto const* CCE = dyn_cast<CXXConstructExpr>(E);
            CCE && CCE->getParenOrBraceRange().isInvalid())
        {
            // look through implicit copy construction from an lvalue of the same type
            E = CCE->getArg(0);
            continue;
        }
        break;
    }

    auto const* DRE = dyn_cast<DeclRefExpr>(E);
    if (!DRE)
    {
        return nullptr;
    }

    auto const* NTTPD = dyn_cast<NonTypeTemplateParmDecl>(DRE->getDecl());
    if (!NTTPD || NTTPD->getDepth() != Depth)
    {
        return nullptr;
    }

    return NTTPD;
}

Decl const*
getParent(Decl const* D)
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

void
getQualifiedName(
    NamedDecl const* ND,
    raw_ostream& stream,
    const PrintingPolicy &policy)
{
    if (auto const* CTS = dyn_cast<ClassTemplateSpecializationDecl>(ND))
    {
        CTS->getSpecializedTemplate()->printQualifiedName(stream, policy);
        TemplateArgumentList const& args = CTS->getTemplateArgs();
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

Decl const*
decayToPrimaryTemplate(Decl const* D)
{
#ifndef NDEBUG
    SmallString<128> symbolName;
    llvm::raw_svector_ostream os(symbolName);
    D->print(os);
    report::debug("symbolName: ", std::string_view(os.str()));
#endif

    Decl const* ID = D;

    // Check parent
    if (CXXRecordDecl const* ClassParent = dyn_cast<CXXRecordDecl>(getParent(ID)))
    {
        if (Decl const* DecayedClassParent = decayToPrimaryTemplate(ClassParent);
            DecayedClassParent != ClassParent &&
            isa<ClassTemplateSpecializationDecl>(DecayedClassParent))
        {
            auto const* RD = dyn_cast<ClassTemplateDecl>(DecayedClassParent);
            CXXRecordDecl* RDParent = RD->getTemplatedDecl();
            auto* NamedID = dyn_cast<NamedDecl>(ID);
            auto NamedDecls = RDParent->decls()
                | std::ranges::views::transform([](Decl* C) { return dyn_cast<NamedDecl>(C); })
                | std::ranges::views::filter([](NamedDecl* C) { return C; });
            for (NamedDecl const* Child : NamedDecls)
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
    if (auto const* TSD = dynamic_cast<ClassTemplateSpecializationDecl const*>(ID);
        TSD &&
        !TSD->isExplicitSpecialization())
    {
        ID = TSD->getSpecializedTemplate();
    }

    return ID;
}

bool
isAllImplicit(Decl const* D)
{
    if (!D)
    {
        return true;
    }
    if (auto const* TSD = dynamic_cast<ClassTemplateSpecializationDecl const*>(D);
        TSD &&
        TSD->isExplicitSpecialization())
    {
        return false;
    }
    if (auto const* TSD = dynamic_cast<VarTemplateSpecializationDecl const*>(D);
        TSD &&
        TSD->isExplicitSpecialization())
    {
        return false;
    }
    auto const* P = getParent(D);
    return isAllImplicit(P);
}

bool
isVirtualMember(Decl const* D)
{
    if (auto const* MD = dyn_cast<CXXMethodDecl>(D))
    {
        return MD->isVirtual();
    }
    return false;
}

bool
isAnonymousNamespace(const Decl *D)
{
    if (auto const* ND = dyn_cast<NamespaceDecl>(D))
    {
        return ND->isAnonymousNamespace();
    }
    return false;
}

bool
isStaticFileLevelMember(const Decl *D)
{
    if (const auto *VD = dyn_cast<VarDecl>(D)) {
        return VD->getStorageClass() == SC_Static && VD->getDeclContext()->isFileContext();
    }
    if (const auto *FD = dyn_cast<FunctionDecl>(D)) {
        return FD->getStorageClass() == SC_Static && FD->getDeclContext()->isFileContext();
    }
    return false;
}


} // clang::mrdocs
