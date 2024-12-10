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
    : public DeclVisitor<InstantiatedFromVisitor, Decl*>
{
public:
    Decl*
    VisitDecl(Decl* D)
    {
        return D;
    }

    FunctionDecl*
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

    CXXRecordDecl*
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

    VarDecl*
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

    TypedefNameDecl*
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

    FunctionDecl*
    VisitFunctionDecl(FunctionDecl* D)
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

    CXXRecordDecl*
    VisitClassTemplatePartialSpecializationDecl(ClassTemplatePartialSpecializationDecl* D)
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

    CXXRecordDecl*
    VisitClassTemplateSpecializationDecl(ClassTemplateSpecializationDecl* D)
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

    CXXRecordDecl*
    VisitCXXRecordDecl(CXXRecordDecl* D)
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

    VarDecl*
    VisitVarTemplatePartialSpecializationDecl(VarTemplatePartialSpecializationDecl* D)
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

    VarDecl*
    VisitVarTemplateSpecializationDecl(VarTemplateSpecializationDecl* D)
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

    VarDecl*
    VisitVarDecl(VarDecl* D)
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

    EnumDecl*
    VisitEnumDecl(EnumDecl* D)
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

    TypedefNameDecl*
    VisitTypedefNameDecl(TypedefNameDecl* D)
    {
        DeclContext* Context = D->getNonTransparentDeclContext();
        if (Context->isFileContext())
        {
            return D;
        }
        auto const* ContextPattern =
            cast<DeclContext>(Visit(cast<Decl>(Context)));
        if (Context == ContextPattern)
        {
            return D;
        }
        for (auto lookup = ContextPattern->lookup(D->getDeclName());
             NamedDecl * ND : lookup)
        {
            if (auto* TND = dyn_cast<TypedefNameDecl>(ND))
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
