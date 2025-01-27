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

namespace clang::mrdocs {

/** A visitor class for handling instantiations from templates.

    This class provides methods to visit various template declarations
    and retrieve the original declarations from which they were instantiated.
 */
class InstantiatedFromVisitor
    : public ConstDeclVisitor<InstantiatedFromVisitor, Decl const*>
{
public:
    Decl const*
    VisitDecl(Decl const* D)
    {
        return D;
    }

    FunctionDecl const*
    VisitFunctionTemplateDecl(FunctionTemplateDecl const* D)
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

    CXXRecordDecl const*
    VisitClassTemplateDecl(ClassTemplateDecl const* D)
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

    VarDecl const*
    VisitVarTemplateDecl(VarTemplateDecl const* D)
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

    TypedefNameDecl const*
    VisitTypeAliasTemplateDecl(TypeAliasTemplateDecl const* D)
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

    FunctionDecl const*
    VisitFunctionDecl(FunctionDecl const* D)
    {
        if (FunctionDecl const* DD = nullptr;
            D->isDefined(DD, false))
        {
            D = const_cast<FunctionDecl*>(DD);
        }

        if (MemberSpecializationInfo const* MSI = D->getMemberSpecializationInfo())
        {
            if (!MSI->isExplicitSpecialization())
            {
                D = cast<FunctionDecl>(MSI->getInstantiatedFrom());
            }
        }
        else if(D->getTemplateSpecializationKind() !=
            TSK_ExplicitSpecialization)
        {
            D = D->getFirstDecl();
            if (auto* FTD = D->getPrimaryTemplate())
            {
                D = VisitFunctionTemplateDecl(FTD);
            }
        }
        return D;
    }

    CXXRecordDecl const*
    VisitClassTemplatePartialSpecializationDecl(ClassTemplatePartialSpecializationDecl const* D)
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

    CXXRecordDecl const*
    VisitClassTemplateSpecializationDecl(ClassTemplateSpecializationDecl const* D)
    {
        if (!D->isExplicitSpecialization())
        {
            auto const inst_from = D->getSpecializedTemplateOrPartial();
            if(auto* CTPSD = inst_from.dyn_cast<
                ClassTemplatePartialSpecializationDecl*>())
            {
                MRDOCS_ASSERT(D != CTPSD);
                return VisitClassTemplatePartialSpecializationDecl(CTPSD);
            }
            // Explicit instantiation declaration/definition
            else if(auto* CTD = inst_from.dyn_cast<ClassTemplateDecl*>())
            {
                return VisitClassTemplateDecl(CTD);
            }
        }
        return VisitCXXRecordDecl(D);
    }

    CXXRecordDecl const*
    VisitCXXRecordDecl(CXXRecordDecl const* D)
    {
        while (MemberSpecializationInfo const* MSI =
            D->getMemberSpecializationInfo())
        {
            // if this is a member of an explicit specialization,
            // then we have the correct declaration
            if (MSI->isExplicitSpecialization())
            {
                break;
            }
            D = cast<CXXRecordDecl>(MSI->getInstantiatedFrom());
        }
        return D;
    }

    VarDecl const*
    VisitVarTemplatePartialSpecializationDecl(VarTemplatePartialSpecializationDecl const* D)
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

    VarDecl const*
    VisitVarTemplateSpecializationDecl(VarTemplateSpecializationDecl const* D)
    {
        if(! D->isExplicitSpecialization())
        {
            auto const inst_from = D->getSpecializedTemplateOrPartial();
            if(auto* VTPSD = inst_from.dyn_cast<
                VarTemplatePartialSpecializationDecl*>())
            {
                MRDOCS_ASSERT(D != VTPSD);
                return VisitVarTemplatePartialSpecializationDecl(VTPSD);
            }
            // explicit instantiation declaration/definition
            else if(auto* VTD = inst_from.dyn_cast<
                VarTemplateDecl*>())
            {
                return VisitVarTemplateDecl(VTD);
            }
        }
        return VisitVarDecl(D);
    }

    VarDecl const*
    VisitVarDecl(VarDecl const* D)
    {
        while(MemberSpecializationInfo* MSI =
            D->getMemberSpecializationInfo())
        {
            if (MSI->isExplicitSpecialization())
            {
                break;
            }
            D = cast<VarDecl>(MSI->getInstantiatedFrom());
        }
        return D;
    }

    EnumDecl const*
    VisitEnumDecl(EnumDecl const* D)
    {
        while(MemberSpecializationInfo* MSI =
            D->getMemberSpecializationInfo())
        {
            if (MSI->isExplicitSpecialization())
            {
                break;
            }
            D = cast<EnumDecl>(MSI->getInstantiatedFrom());
        }
        return D;
    }

    TypedefNameDecl const*
    VisitTypedefNameDecl(TypedefNameDecl const* D)
    {
        DeclContext const* Context = D->getNonTransparentDeclContext();
        if (Context->isFileContext())
        {
            return D;
        }
        Decl const* ContextDecl = Decl::castFromDeclContext(Context);
        Decl const* ContextInstatiationContextDecl = Visit(ContextDecl);
        DeclContext const* ContextPattern =
            Decl::castToDeclContext(ContextInstatiationContextDecl);
        if (Context == ContextPattern)
        {
            return D;
        }
        for (auto lookup = ContextPattern->lookup(D->getDeclName());
             NamedDecl * ND : lookup)
        {
            if (auto const* TND = dyn_cast<TypedefNameDecl>(ND))
            {
                return TND;
            }
            if (auto const* TATD = dyn_cast<TypeAliasTemplateDecl>(ND))
            {
                return TATD->getTemplatedDecl();
            }
        }
        return D;
    }
};

} // clang::mrdocs

#endif
