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

#ifndef MRDOCS_LIB_AST_INSTANTIATEDFROMVISITOR_HPP
#define MRDOCS_LIB_AST_INSTANTIATEDFROMVISITOR_HPP

#include <clang/AST/DeclVisitor.h>

namespace mrdocs {

/** A visitor class for handling instantiations from templates.

    This class provides methods to visit various template declarations
    and retrieve the original declarations from which they were instantiated.
 */
class InstantiatedFromVisitor
    : public clang::ConstDeclVisitor<InstantiatedFromVisitor, clang::Decl const*>
{
public:
    clang::Decl const*
    VisitDecl(clang::Decl const* D)
    {
        return D;
    }

    clang::FunctionDecl const*
    VisitFunctionTemplateDecl(clang::FunctionTemplateDecl const* D)
    {
        while(auto* MT = D->getInstantiatedFromMemberTemplate())
        {
            if (D->isMemberSpecialization())
            {
                break;
            }
            D = MT;
        }
        return D->getTemplatedDecl();
    }

    clang::CXXRecordDecl const*
    VisitClassTemplateDecl(clang::ClassTemplateDecl const* D)
    {
        while (auto* MT = D->getInstantiatedFromMemberTemplate())
        {
            if (D->isMemberSpecialization())
            {
                break;
            }
            D = MT;
        }
        return D->getTemplatedDecl();
    }

    clang::VarDecl const*
    VisitVarTemplateDecl(clang::VarTemplateDecl const* D)
    {
        while(auto* MT = D->getInstantiatedFromMemberTemplate())
        {
            if (D->isMemberSpecialization())
            {
                break;
            }
            D = MT;
        }
        return D->getTemplatedDecl();
    }

    clang::TypedefNameDecl const*
    VisitTypeAliasTemplateDecl(clang::TypeAliasTemplateDecl const* D)
    {
        if(auto* MT = D->getInstantiatedFromMemberTemplate())
        {
            // KRYSTIAN NOTE: we don't really need to check this
            if (!D->isMemberSpecialization())
            {
                D = MT;
            }
        }
        return VisitTypedefNameDecl(D->getTemplatedDecl());
    }

    clang::FunctionDecl const*
    VisitFunctionDecl(clang::FunctionDecl const* D)
    {
        if (clang::FunctionDecl const* DD = nullptr;
            D->isDefined(DD, false))
        {
            D = const_cast<clang::FunctionDecl*>(DD);
        }

        if (clang::MemberSpecializationInfo const* MSI = D->getMemberSpecializationInfo())
        {
            if (!MSI->isExplicitSpecialization())
            {
                D = cast<clang::FunctionDecl>(MSI->getInstantiatedFrom());
            }
        }
        else if(D->getTemplateSpecializationKind() !=
            clang::TSK_ExplicitSpecialization)
        {
            D = D->getFirstDecl();
            if (auto* FTD = D->getPrimaryTemplate())
            {
                D = VisitFunctionTemplateDecl(FTD);
            }
        }
        return D;
    }

    clang::CXXRecordDecl const*
    VisitClassTemplatePartialSpecializationDecl(clang::ClassTemplatePartialSpecializationDecl const* D)
    {
        while (auto* MT = D->getInstantiatedFromMember())
        {
            if (D->isMemberSpecialization())
            {
                break;
            }
            D = MT;
        }
        return VisitClassTemplateSpecializationDecl(D);
    }

    clang::CXXRecordDecl const*
    VisitClassTemplateSpecializationDecl(clang::ClassTemplateSpecializationDecl const* D)
    {
        if (!D->isExplicitSpecialization())
        {
            auto const inst_from = D->getSpecializedTemplateOrPartial();
            if(auto* CTPSD = inst_from.dyn_cast<
                clang::ClassTemplatePartialSpecializationDecl*>())
            {
                MRDOCS_ASSERT(D != CTPSD);
                return VisitClassTemplatePartialSpecializationDecl(CTPSD);
            }
            // Explicit instantiation declaration/definition
            else if(auto* CTD = inst_from.dyn_cast<clang::ClassTemplateDecl*>())
            {
                return VisitClassTemplateDecl(CTD);
            }
        }
        return VisitCXXRecordDecl(D);
    }

    clang::CXXRecordDecl const*
    VisitCXXRecordDecl(clang::CXXRecordDecl const* D)
    {
        while (clang::MemberSpecializationInfo const* MSI =
            D->getMemberSpecializationInfo())
        {
            // if this is a member of an explicit specialization,
            // then we have the correct declaration
            if (MSI->isExplicitSpecialization())
            {
                break;
            }
            D = cast<clang::CXXRecordDecl>(MSI->getInstantiatedFrom());
        }
        return D;
    }

    clang::VarDecl const*
    VisitVarTemplatePartialSpecializationDecl(clang::VarTemplatePartialSpecializationDecl const* D)
    {
        while(auto* MT = D->getInstantiatedFromMember())
        {
            if (D->isMemberSpecialization())
            {
                break;
            }
            D = MT;
        }
        return VisitVarTemplateSpecializationDecl(D);
    }

    clang::VarDecl const*
    VisitVarTemplateSpecializationDecl(clang::VarTemplateSpecializationDecl const* D)
    {
        if(! D->isExplicitSpecialization())
        {
            auto const inst_from = D->getSpecializedTemplateOrPartial();
            if(auto* VTPSD = inst_from.dyn_cast<
                clang::VarTemplatePartialSpecializationDecl*>())
            {
                MRDOCS_ASSERT(D != VTPSD);
                return VisitVarTemplatePartialSpecializationDecl(VTPSD);
            }
            // explicit instantiation declaration/definition
            else if(auto* VTD = inst_from.dyn_cast<
                clang::VarTemplateDecl*>())
            {
                return VisitVarTemplateDecl(VTD);
            }
        }
        return VisitVarDecl(D);
    }

    clang::VarDecl const*
    VisitVarDecl(clang::VarDecl const* D)
    {
        while(clang::MemberSpecializationInfo* MSI =
            D->getMemberSpecializationInfo())
        {
            if (MSI->isExplicitSpecialization())
            {
                break;
            }
            D = cast<clang::VarDecl>(MSI->getInstantiatedFrom());
        }
        return D;
    }

    clang::EnumDecl const*
    VisitEnumDecl(clang::EnumDecl const* D)
    {
        while(clang::MemberSpecializationInfo* MSI =
            D->getMemberSpecializationInfo())
        {
            if (MSI->isExplicitSpecialization())
            {
                break;
            }
            D = cast<clang::EnumDecl>(MSI->getInstantiatedFrom());
        }
        return D;
    }

    clang::TypedefNameDecl const*
    VisitTypedefNameDecl(clang::TypedefNameDecl const* D)
    {
        clang::DeclContext const* Context = D->getNonTransparentDeclContext();
        if (Context->isFileContext())
        {
            return D;
        }
        clang::Decl const* ContextDecl = clang::Decl::castFromDeclContext(Context);
        clang::Decl const* ContextInstatiationContextDecl = Visit(ContextDecl);
        clang::DeclContext const* ContextPattern =
            clang::Decl::castToDeclContext(ContextInstatiationContextDecl);
        if (Context == ContextPattern)
        {
            return D;
        }
        for (auto lookup = ContextPattern->lookup(D->getDeclName());
             clang::NamedDecl * ND : lookup)
        {
            if (auto const* TND = dyn_cast<clang::TypedefNameDecl>(ND))
            {
                return TND;
            }
            if (auto const* TATD = dyn_cast<clang::TypeAliasTemplateDecl>(ND))
            {
                return TATD->getTemplatedDecl();
            }
        }
        return D;
    }
};

} // mrdocs

#endif
