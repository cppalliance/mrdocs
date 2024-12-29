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

/** A visitor to build objects from `Type`s.

    MrDocs might need to convert class instances derived from `Type`
    into other struct instances like `TypeInfo` or `NameInfo`.

    This class can be used to define a visitor to build objects
    from `Type`s. The visitor can be defined as a class that
    derives from `TerminalTypeVisitor<Derived>`:

    @code
    class SomeTypeBuilder
        : public TerminalTypeVisitor<SomeTypeBuilder> {
    public:
        void build{DerivedType}({DerivedType}* T);
        // ...
        void buildTerminal(...) { ... }
        // ...
        void populate(const FunctionType* T);
    };
    @endcode

    When `SomeTypeBuilder::Visit` is called, the `Type` is cast
    to the derived class type, any corresponding information is gathered,
    and `Visit()` is called again with internal types until we reach
    a terminal type. When a terminal type is reached, the corresponding
    `build{DerivedType}` function or a `buildTerminal` overload is called.

    This base class implements the common functionality to
    visit different types and build the corresponding objects,
    so that only the specific `build{DerivedType}` functions
    need to be implemented by the derived class.

    It inherits the `bool Visit(const Type *T)` function
    from `TerminalTypeVisitor<Derived>`, which converts the
    `Type` into the concrete type and calls the corresponding
    `VisitXXXType` function. It also provides
    `bool Visit(QualType QT)` as an extension to visit the
    `Type` associated with the qualified type.

    Each `VisitXXXType` function will store any relative information
    about that type and call the corresponding `VisitXXXType` function
    for the internal type. For instance, `VisitUsingType` will
    call `Visit(T->getUnderlyingType())`, and so on, until we
    reach terminal types.

    This process will continue recursively until `VisitXXXType`
    we reach a terminal type, such as `VisitPointerType`.

    @tparam Derived The derived class type.
 */
template <class Derived>
class TerminalTypeVisitor
    : public TypeVisitor<TerminalTypeVisitor<Derived>, bool>
{
    friend class TerminalTypeVisitor::TypeVisitor;

    // The ASTVisitor instance.
    ASTVisitor& Visitor_;

    // The local qualifiers.
    unsigned Quals_ = 0;

    // Whether the type is a pack expansion.
    bool IsPack_ = false;

    // The optional NestedNameSpecifier.
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

    /** Visit a Type.

        This function casts the given Type to the derived class type
        and calls the corresponding `VisitXXXType` function.
     */
    using TerminalTypeVisitor::TypeVisitor::Visit;

    /** Visit a Qualified Type.

        This function stores the local qualifiers of the given
        Qualified Type and calls the corresponding `VisitXXXType`
        function for the associated `Type`.
     */
    bool
    Visit(QualType const QT)
    {
        Quals_ |= QT.getLocalFastQualifiers();
        Type const* T = QT.getTypePtrOrNull();
        return Visit(T);
    }

    /** Get the ASTVisitor instance.

        This function returns a reference to the ASTVisitor instance.

        @return A reference to the ASTVisitor instance.
     */
    ASTVisitor&
    getASTVisitor() const
    {
        return Visitor_;
    }

    /** A placeholder for `buildPointer`

        This function is an empty placeholder for `buildPointer` when
        not defined by the `Derived` visitor.
     */
    void
    buildPointer
        (const PointerType*,
        unsigned)
    {
    }

    /** A placeholder for `buildLValueRef

        This function is an empty placeholder for `buildLValueReference` when
        not defined by the `Derived` visitor.
     */
    void
    buildLValueReference(
        const LValueReferenceType*)
    {
    }

    /** A placeholder for `buildRValueRef

        This function is an empty placeholder for `buildRValueReference` when
        not defined by the `Derived` visitor.
     */
    void
    buildRValueReference(
        const RValueReferenceType*)
    {
    }

    /** A placeholder for `buildMemberPoi

        This function is an empty placeholder for `buildMemberPointer` when
        not defined by the `Derived` visitor.
     */
    void
    buildMemberPointer(
        const MemberPointerType*, unsigned)
    {
    }

    /** A placeholder for `buildArray` wh

        This function is an empty placeholder for `buildArray` when
        not defined by the `Derived` visitor.
     */
    void
    buildArray(
        const ArrayType*)
    {
    }

    void
    populate(
        const FunctionType*)
    {
    }

    /** A placeholder for `buildDecltype`

        This function is an empty placeholder for `buildDecltype` when
        not defined by the `Derived` visitor.
     */
    void
    buildDecltype(
        const DecltypeType*,
        unsigned,
        bool)
    {
    }

    /** A placeholder for `buildAuto` whe

        This function is an empty placeholder for `buildAuto` when
        not defined by the `Derived` visitor.
     */
    void
    buildAuto(
        const AutoType*,
        unsigned,
        bool)
    {
    }

    /** A placeholder for `buildTerminal`

        This function is an empty placeholder for `buildTerminal` when
        not defined by the `Derived` visitor.
     */
    void
    buildTerminal(
        NestedNameSpecifier const*,
        Type const*,
        unsigned,
        bool)
    {
    }

    /** A placeholder for `buildTerminal`

        This function is an empty placeholder for `buildTerminal` when
        not defined by the `Derived` visitor.
     */
    void
    buildTerminal(
        NestedNameSpecifier const*,
        IdentifierInfo const*,
        std::optional<ArrayRef<TemplateArgument>>,
        unsigned,
        bool)
    {
    }

    /** A placeholder for `buildTerminal`

        This function is an empty placeholder for `buildTerminal` when
        not defined by the `Derived` visitor.
     */
    void
    buildTerminal(
        NestedNameSpecifier const*,
        NamedDecl*,
        std::optional<ArrayRef<TemplateArgument>>,
        unsigned,
        bool)
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

    // A type with parentheses, e.g., `(int)`.
    bool
    VisitParenType(
        const ParenType* T)
    {
        QualType I = T->getInnerType();
        return Visit(I);
    }

    bool
    VisitMacroQualified(
        const MacroQualifiedType* T)
    {
        QualType UT = T->getUnderlyingType();
        return Visit(UT);
    }

    bool
    VisitAttributedType(
        const AttributedType* T)
    {
        QualType MT = T->getModifiedType();
        return Visit(MT);
    }

    bool
    VisitAdjustedType(
        const AdjustedType* T)
    {
        QualType OT = T->getOriginalType();
        return Visit(OT);
    }

    bool
    VisitUsingType(
        const UsingType* T)
    {
        QualType UT = T->getUnderlyingType();
        return Visit(UT);
    }

    bool
    VisitSubstTemplateTypeParmType(
        const SubstTemplateTypeParmType* T)
    {
        QualType RT = T->getReplacementType();
        return Visit(RT);
    }

    // ----------------------------------------------------------------

    // A type that was referred to using an elaborated type keyword,
    // e.g., `struct S`, or via a qualified name, e.g., `N::M::type`.
    bool
    VisitElaboratedType(
        const ElaboratedType* T)
    {
        NNS_ = T->getQualifier();
        QualType NT = T->getNamedType();
        return Visit(NT);
    }

    bool
    VisitPackExpansionType(
        const PackExpansionType* T)
    {
        IsPack_ = true;
        QualType PT = T->getPattern();
        return Visit(PT);
    }

    // ----------------------------------------------------------------

    bool
    VisitPointerType(
        const PointerType* T)
    {
        getDerived().buildPointer(T, std::exchange(Quals_, 0));
        QualType PT = T->getPointeeType();
        return Visit(PT);
    }

    bool
    VisitLValueReferenceType(
        const LValueReferenceType* T)
    {
        getDerived().buildLValueReference(T);
        Quals_ = 0;
        QualType PT = T->getPointeeType();
        return Visit(PT);
    }

    bool
    VisitRValueReferenceType(
        const RValueReferenceType* T)
    {
        getDerived().buildRValueReference(T);
        Quals_ = 0;
        QualType PT = T->getPointeeType();
        return Visit(PT);
    }

    bool
    VisitMemberPointerType(
        const MemberPointerType* T)
    {
        getDerived().buildMemberPointer(T, std::exchange(Quals_, 0));
        QualType PT = T->getPointeeType();
        return Visit(PT);
    }

    bool
    VisitFunctionType(
        const FunctionType* T)
    {
        getDerived().populate(T);
        QualType RT = T->getReturnType();
        return Visit(RT);
    }

    bool
    VisitArrayType(
        const ArrayType* T)
    {
        getDerived().buildArray(T);
        QualType ET = T->getElementType();
        return Visit(ET);
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
        // to represent deduced types also stores what it
        // was deduced as.
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

    // Visit a template specialization such as `A<T>`
    bool
    VisitTemplateSpecializationType(
        TemplateSpecializationType const* T)
    {
        if (auto SFINAE = getASTVisitor().isSFINAEType(T); SFINAE.has_value())
        {
            return getDerived().Visit(SFINAE->first);
        }

        // In most cases, a template name is simply a reference
        // to a class template. In `X<int> xi;` the template name
        // is `template<typename T> class X { };`.
        // Template names can also refer to function templates,
        // C++0x template aliases, etc...
        TemplateName const TN = T->getTemplateName();
        MRDOCS_ASSERT(! TN.isNull());

        // The list of template parameters and a reference to
        // the templated scoped declaration
        NamedDecl* D = TN.getAsTemplateDecl();

        if (!T->isTypeAlias())
        {
            auto* CT = T->getCanonicalTypeInternal().getTypePtrOrNull();
            if (auto* ICT = dyn_cast_or_null<InjectedClassNameType>(CT))
            {
                D = ICT->getDecl();
            }
            else if (auto* RT = dyn_cast_or_null<RecordType>(CT))
            {
                D = RT->getDecl();
            }
        }

        getDerived().buildTerminal(
            NNS_, D,
            T->template_arguments(),
            Quals_, IsPack_);
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
