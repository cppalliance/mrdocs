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

#ifndef MRDOCS_LIB_AST_TERMINALTYPEVISITOR_HPP
#define MRDOCS_LIB_AST_TERMINALTYPEVISITOR_HPP

#include <lib/AST/ASTVisitor.hpp>
#include <clang/AST/TypeVisitor.h>

namespace clang::mrdocs {

/** A visitor class for terminal types.

    This class is used to visit various terminal types in the AST.
    It derives from the TypeVisitor class and provides specific
    implementations for visiting different types.

    @tparam Derived The derived class type.
 */
template <class Derived>
class TerminalTypeVisitor
    : public TypeVisitor<TerminalTypeVisitor<Derived>, bool>
{
    friend class TerminalTypeVisitor::TypeVisitor;

    ASTVisitor& Visitor_;

    unsigned Quals_ = 0;
    bool IsPack_ = false;
    const NestedNameSpecifier* NNS_;

public:
    /** Constructor for TerminalTypeVisitor.

        This constructor initializes the TerminalTypeVisitor with the given ASTVisitor
        and an optional NestedNameSpecifier.

        @param Visitor The ASTVisitor instance.
        @param NNS The optional NestedNameSpecifier.
     */
    explicit
    TerminalTypeVisitor(
        ASTVisitor& Visitor,
        const NestedNameSpecifier* NNS = nullptr)
        : Visitor_(Visitor)
        , NNS_(NNS)
    {
    }

    /** Get the ASTVisitor instance.

        This function returns a reference to the ASTVisitor instance.

        @return A reference to the ASTVisitor instance.
     */
    ASTVisitor&
    getASTVisitor()
    {
        return Visitor_;
    }

    using TerminalTypeVisitor::TypeVisitor::Visit;

    bool
    Visit(QualType const QT)
    {
        Quals_ |= QT.getLocalFastQualifiers();
        return Visit(QT.getTypePtrOrNull());
    }

    void
    buildPointer
        (const PointerType* T,
        unsigned quals)
    {
    }

    void
    buildLValueReference(
        const LValueReferenceType* T)
    {
    }

    void
    buildRValueReference(
        const RValueReferenceType* T)
    {
    }

    void
    buildMemberPointer(
        const MemberPointerType* T, unsigned quals)
    {
    }

    void
    buildArray(
        const ArrayType* T)
    {
    }

    void
    populate(
        const FunctionType* T)
    {
    }

    void
    buildDecltype(
        const DecltypeType* T,
        unsigned quals,
        bool pack)
    {
    }

    void
    buildAuto(
        const AutoType* T,
        unsigned quals,
        bool pack)
    {
    }

    void
    buildTerminal(
        const NestedNameSpecifier* NNS,
        const Type* T,
        unsigned quals,
        bool pack)
    {
    }

    void
    buildTerminal(
        const NestedNameSpecifier* NNS,
        const IdentifierInfo* II,
        std::optional<ArrayRef<TemplateArgument>> TArgs,
        unsigned quals,
        bool pack)
    {
    }

    void
    buildTerminal(
        const NestedNameSpecifier* NNS,
        const NamedDecl* D,
        std::optional<ArrayRef<TemplateArgument>> TArgs,
        unsigned quals,
        bool pack)
    {
    }


private:
    /** Get the derived class instance.

        This function casts the current instance to the derived class type.

        @return A reference to the derived class instance.
     */
    Derived&
    getDerived()
    {
        return static_cast<Derived&>(*this);
    }

    bool
    VisitParenType(
        const ParenType* T)
    {
        return Visit(T->getInnerType());
    }

    bool
    VisitMacroQualified(
        const MacroQualifiedType* T)
    {
        return Visit(T->getUnderlyingType());
    }

    bool
    VisitAttributedType(
        const AttributedType* T)
    {
        return Visit(T->getModifiedType());
    }

    bool
    VisitAdjustedType(
        const AdjustedType* T)
    {
        return Visit(T->getOriginalType());
    }

    bool
    VisitUsingType(
        const UsingType* T)
    {
        return Visit(T->getUnderlyingType());
    }

    bool
    VisitSubstTemplateTypeParmType(
        const SubstTemplateTypeParmType* T)
    {
        return Visit(T->getReplacementType());
    }

    // ----------------------------------------------------------------

    bool
    VisitElaboratedType(
        const ElaboratedType* T)
    {
        NNS_ = T->getQualifier();
        return Visit(T->getNamedType());
    }

    bool
    VisitPackExpansionType(
        const PackExpansionType* T)
    {
        IsPack_ = true;
        return Visit(T->getPattern());
    }

    // ----------------------------------------------------------------

    bool
    VisitPointerType(
        const PointerType* T)
    {
        getDerived().buildPointer(T, std::exchange(Quals_, 0));
        return Visit(T->getPointeeType());
    }

    bool
    VisitLValueReferenceType(
        const LValueReferenceType* T)
    {
        getDerived().buildLValueReference(T);
        Quals_ = 0;
        return Visit(T->getPointeeType());
    }

    bool
    VisitRValueReferenceType(
        const RValueReferenceType* T)
    {
        getDerived().buildRValueReference(T);
        Quals_ = 0;
        return Visit(T->getPointeeType());
    }

    bool
    VisitMemberPointerType(
        const MemberPointerType* T)
    {
        getDerived().buildMemberPointer(T, std::exchange(Quals_, 0));
        return Visit(T->getPointeeType());
    }

    bool
    VisitFunctionType(
        const FunctionType* T)
    {
        getDerived().populate(T);
        return Visit(T->getReturnType());
    }

    bool
    VisitArrayType(
        const ArrayType* T)
    {
        getDerived().buildArray(T);
        return Visit(T->getElementType());
    }

    // ----------------------------------------------------------------

    bool
    VisitDecltypeType(
        const DecltypeType* T)
    {
        getDerived().buildDecltype(T, Quals_, IsPack_);
        return true;
    }

    bool
    VisitAutoType(
        const AutoType* T)
    {
        // KRYSTIAN NOTE: we don't use isDeduced because it will
        // return true if the type is dependent
        // if the type has been deduced, use the deduced type
        getDerived().buildAuto(T, Quals_, IsPack_);
        return true;
    }

    bool
    VisitDeducedTemplateSpecializationType(
        const DeducedTemplateSpecializationType* T)
    {
        // KRYSTIAN TODO: we should probably add a TypeInfo
        // to represent deduced types that also stores what
        // it was deduced as.
        if (QualType DT = T->getDeducedType(); !DT.isNull())
        {
            return Visit(DT);
        }
        TemplateName const TN = T->getTemplateName();
        MRDOCS_ASSERT(! TN.isNull());
        NamedDecl* ND = TN.getAsTemplateDecl();
        getDerived().buildTerminal(NNS_, ND,
            std::nullopt, Quals_, IsPack_);
        return true;
    }

    bool
    VisitDependentNameType(
        const DependentNameType* T)
    {
        if (auto SFINAE = getASTVisitor().isSFINAEType(T); SFINAE.has_value())
        {
            return getDerived().Visit(SFINAE->first);
        }

        if (auto const* NNS = T->getQualifier())
        {
            NNS_ = NNS;
        }
        getDerived().buildTerminal(NNS_, T->getIdentifier(),
            std::nullopt, Quals_, IsPack_);
        return true;
    }

    bool
    VisitDependentTemplateSpecializationType(
        const DependentTemplateSpecializationType* T)
    {
        if (auto const* NNS = T->getQualifier())
        {
            NNS_ = NNS;
        }
        getDerived().buildTerminal(NNS_, T->getIdentifier(),
            T->template_arguments(), Quals_, IsPack_);
        return true;
    }

    bool
    VisitTemplateSpecializationType(
        const TemplateSpecializationType* T)
    {
        if (auto SFINAE = getASTVisitor().isSFINAEType(T); SFINAE.has_value())
        {
            return getDerived().Visit(SFINAE->first);
        }

        TemplateName const TN = T->getTemplateName();
        MRDOCS_ASSERT(! TN.isNull());
        NamedDecl* ND = TN.getAsTemplateDecl();
        if(! T->isTypeAlias())
        {
            auto* CT = T->getCanonicalTypeInternal().getTypePtrOrNull();
            if (auto* ICT = dyn_cast_or_null<InjectedClassNameType>(CT))
            {
                ND = ICT->getDecl();
            }
            else if (auto* RT = dyn_cast_or_null<RecordType>(CT))
            {
                ND = RT->getDecl();
            }
        }
        getDerived().buildTerminal(NNS_, ND,
            T->template_arguments(), Quals_, IsPack_);
        return true;
    }

    bool
    VisitRecordType(
        const RecordType* T)
    {
        RecordDecl* RD = T->getDecl();
        // if this is an instantiation of a class template,
        // create a SpecializationTypeInfo & extract the template arguments
        std::optional<ArrayRef<TemplateArgument>> TArgs = std::nullopt;
        if (auto const* CTSD = dyn_cast<ClassTemplateSpecializationDecl>(RD))
        {
            TArgs = CTSD->getTemplateArgs().asArray();
        }
        getDerived().buildTerminal(NNS_, RD,
            TArgs, Quals_, IsPack_);
        return true;
    }

    bool
    VisitInjectedClassNameType(
        const InjectedClassNameType* T)
    {
        getDerived().buildTerminal(NNS_, T->getDecl(),
            std::nullopt, Quals_, IsPack_);
        return true;
    }

    bool
    VisitEnumType(
        const EnumType* T)
    {
        getDerived().buildTerminal(NNS_, T->getDecl(),
            std::nullopt, Quals_, IsPack_);
        return true;
    }

    bool
    VisitTypedefType(
        const TypedefType* T)
    {
        getDerived().buildTerminal(NNS_, T->getDecl(),
            std::nullopt, Quals_, IsPack_);
        return true;
    }

    bool
    VisitTemplateTypeParmType(
        const TemplateTypeParmType* T)
    {
        const IdentifierInfo* II = nullptr;
        if (TemplateTypeParmDecl const* D = T->getDecl())
        {
            if(D->isImplicit())
            {
                // special case for implicit template parameters
                // resulting from abbreviated function templates
                getDerived().buildTerminal(
                    NNS_, T, Quals_, IsPack_);
                return true;
            }
            II = D->getIdentifier();
        }
        getDerived().buildTerminal(NNS_, II,
            std::nullopt, Quals_, IsPack_);
        return true;
    }

    bool
    VisitSubstTemplateTypeParmPackType(
        const SubstTemplateTypeParmPackType* T)
    {
        getDerived().buildTerminal(NNS_, T->getIdentifier(),
            std::nullopt, Quals_, IsPack_);
        return true;
    }

    bool
    VisitType(const Type* T)
    {
        getDerived().buildTerminal(
            NNS_, T, Quals_, IsPack_);
        return true;
    }

};

} // clang::mrdocs

#endif
