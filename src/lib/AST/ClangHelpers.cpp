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

#include <lib/AST/ClangHelpers.hpp>
#include <mrdocs/Support/Assert.hpp>
#include <mrdocs/Support/Report.hpp>
#include <clang/Driver/Driver.h>
#include <clang/Index/USRGeneration.h>
#include <clang/Sema/Template.h>
#include <llvm/Option/ArgList.h>
#include <ranges>

namespace mrdocs {

clang::Expr const*
SubstituteConstraintExpressionWithoutSatisfaction(
    clang::Sema &S, const clang::Sema::TemplateCompareNewDeclInfo &DeclInfo,
    clang::Expr const* ConstrExpr)
{
    clang::MultiLevelTemplateArgumentList MLTAL = S.getTemplateInstantiationArgs(
        DeclInfo.getDecl(), DeclInfo.getLexicalDeclContext(), /*Final=*/false,
        /*Innermost=*/std::nullopt,
        /*RelativeToPrimary=*/true,
        /*Pattern=*/nullptr, /*ForConstraintInstantiation=*/true,
        /*SkipForSpecialization*/ false);

    if (MLTAL.getNumSubstitutedLevels() == 0)
    {
        return ConstrExpr;
    }

    clang::Sema::SFINAETrap const SFINAE(S, /*AccessCheckingSFINAE=*/false);

    clang::Sema::InstantiatingTemplate Inst(
        S, DeclInfo.getLocation(),
        clang::Sema::InstantiatingTemplate::ConstraintNormalization{},
        const_cast<clang::NamedDecl *>(DeclInfo.getDecl()), clang::SourceRange{});
    if (Inst.isInvalid())
    {
        return nullptr;
    }

    // Set up a dummy 'instantiation' scope in the case of reference to function
    // parameters that the surrounding function hasn't been instantiated yet. Note
    // this may happen while we're comparing two templates' constraint
    // equivalence.
    clang::LocalInstantiationScope ScopeForParameters(S);
    if (auto *FD = DeclInfo.getDecl()->getAsFunction())
    {
        for (auto *PVD : FD->parameters())
        {
            ScopeForParameters.InstantiatedLocal(PVD, PVD);
        }
    }

    Optional<clang::Sema::CXXThisScopeRAII> ThisScope;

    // See TreeTransform::RebuildTemplateSpecializationType. A context scope is
    // essential for having an injected class as the canonical type for a template
    // specialization type at the rebuilding stage. This guarantees that, for
    // out-of-line definitions, injected class name types and their equivalent
    // template specializations can be profiled to the same value, which makes it
    // possible that e.g. constraints involving C<Class<T>> and C<Class> are
    // perceived identical.
    Optional<clang::Sema::ContextRAII> ContextScope;
    if (auto *RD = dyn_cast<clang::CXXRecordDecl>(DeclInfo.getDeclContext()))
    {
        ThisScope.emplace(S, const_cast<clang::CXXRecordDecl *>(RD), clang::Qualifiers());
        ContextScope.emplace(S, const_cast<clang::DeclContext *>(cast<clang::DeclContext>(RD)),
                             /*NewThisContext=*/false);
    }
    clang::ExprResult SubstConstr = S.SubstConstraintExprWithoutSatisfaction(
        const_cast<clang::Expr *>(ConstrExpr), MLTAL);
    if (SFINAE.hasErrorOccurred() || !SubstConstr.isUsable())
    {
        return nullptr;
    }
    return SubstConstr.get();
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

clang::RawComment const*
getDocumentation(clang::Decl const* D)
{
    clang::RawComment const* RC =
        D->getASTContext().getRawCommentForDeclNoCache(D);
    if (!RC)
    {
        auto const* TD = dyn_cast<clang::TemplateDecl>(D);
        MRDOCS_CHECK_OR(TD, nullptr);
        clang::NamedDecl const* ND = TD->getTemplatedDecl();
        MRDOCS_CHECK_OR(ND, nullptr);
        RC = ND->getASTContext().getRawCommentForDeclNoCache(ND);
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
