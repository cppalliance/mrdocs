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

namespace mrdocs {

/** A visitor to build objects from `Type`s.

    MrDocs might need to convert class instances derived from `Type`
    into other struct instances like `Type` or `Name`.

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
        void populate(FunctionType const* T);
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
    `bool Visit(clang::QualType QT)` as an extension to visit the
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
    : public clang::TypeVisitor<TerminalTypeVisitor<Derived>, bool>
{
    friend class TerminalTypeVisitor::TypeVisitor;

    // The ASTVisitor instance.
    ASTVisitor& Visitor_;

    // The local qualifiers.
    unsigned Quals_ = 0;

    // Whether the type is a pack expansion.
    bool IsPack_ = false;

protected:
    // Constraints associated with the type (e.g., SFINAE)
    std::vector<ExprInfo> Constraints;

public:
    /** Constructor for TerminalTypeVisitor.

        This constructor initializes the TerminalTypeVisitor with the given ASTVisitor
        and an optional NestedNameSpecifier.

        @param Visitor The ASTVisitor instance.
        @param NNS The optional NestedNameSpecifier.
     */
    explicit
    TerminalTypeVisitor(
        ASTVisitor& Visitor)
        : Visitor_(Visitor)
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

        Example:
        - Wrapped type: `const int`
        - Unwrapped type: `int`
     */
    bool
    Visit(clang::QualType const QT)
    {
        MRDOCS_SYMBOL_TRACE(QT, Visitor_.context_);
        Quals_ |= QT.getLocalFastQualifiers();
        clang::Type const* T = QT.getTypePtrOrNull();
        MRDOCS_SYMBOL_TRACE(T, Visitor_.context_);
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
    static
    void
    buildPointer
        (clang::PointerType const*,
        unsigned)
    {
    }

    /** A placeholder for `buildLValueRef

        This function is an empty placeholder for `buildLValueReference` when
        not defined by the `Derived` visitor.
     */
    void
    buildLValueReference(
        clang::LValueReferenceType const*)
    {
    }

    /** A placeholder for `buildRValueRef

        This function is an empty placeholder for `buildRValueReference` when
        not defined by the `Derived` visitor.
     */
    void
    buildRValueReference(
        clang::RValueReferenceType const*)
    {
    }

    /** A placeholder for `buildMemberPoi

        This function is an empty placeholder for `buildMemberPointer` when
        not defined by the `Derived` visitor.
     */
    void
    buildMemberPointer(
        clang::MemberPointerType const*, unsigned)
    {
    }

    /** A placeholder for `buildArray` wh

        This function is an empty placeholder for `buildArray` when
        not defined by the `Derived` visitor.
     */
    void
    buildArray(
        clang::ArrayType const*)
    {
    }

    void
    populate(
        clang::FunctionType const*)
    {
    }

    /** A placeholder for `buildDecltype`

        This function is an empty placeholder for `buildDecltype` when
        not defined by the `Derived` visitor.
     */
    void
    buildDecltype(
        clang::DecltypeType const*,
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
        clang::AutoType const*,
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
        clang::NestedNameSpecifier,
        clang::Type const*,
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
        clang::NestedNameSpecifier,
        clang::IdentifierInfo const*,
        Optional<llvm::ArrayRef<clang::TemplateArgument>>,
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
        clang::NestedNameSpecifier,
        clang::NamedDecl*,
        Optional<llvm::ArrayRef<clang::TemplateArgument>>,
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

    /** Visit a type with parentheses, e.g., `(int)`.

        This function unwraps the inner type from the parentheses.

        Example:
        - Wrapped type: `(int)`
        - Unwrapped type: `int`
     */
    bool
    VisitParenType(clang::ParenType const* T)
    {
        clang::QualType I = T->getInnerType();
        return Visit(I);
    }

    /** Visit a macro qualified type.

        This function unwraps the underlying type from the macro qualifier.

        Example:
        - Wrapped type: `MACRO_QUALIFIED(int)`
        - Unwrapped type: `int`
     */
    bool
    VisitMacroQualified(
        clang::MacroQualifiedType const* T)
    {
        clang::QualType UT = T->getUnderlyingType();
        return Visit(UT);
    }

    /** Visit an attributed type.

        This function unwraps the modified type from the attribute.

        Example:
        - Wrapped type: `[[attribute]] int`
        - Unwrapped type: `int`
     */
    bool
    VisitAttributedType(
        clang::AttributedType const* T)
    {
        clang::QualType MT = T->getModifiedType();
        return Visit(MT);
    }

    /** Visit an adjusted type.

        This function unwraps the original type from the adjusted type.

        Example:
        - Wrapped type: adjusted/decayed `int*`
        - Unwrapped type: original `int[4]`
     */
    bool
    VisitAdjustedType(clang::AdjustedType const* T)
    {
        clang::QualType OT = T->getOriginalType();
        return Visit(OT);
    }

    /** Visit a using type.

        This function unwraps the underlying type from the using type.

        Example:
        - Wrapped type: `using TypeAlias = int`
        - Unwrapped type: `int`
     */
    bool
    VisitUsingType(clang::UsingType const* T)
    {
        clang::QualType UT = T->desugar();
        return Visit(UT);
    }

    /** Visit a substituted template type parameter type.

        This function unwraps the replacement type from the substituted template type parameter.

        Example:
        - Wrapped type: `T`
        - Unwrapped type: `int` (if `T` is substituted with `int`)
     */
    bool
    VisitSubstTemplateTypeParmType(
        clang::SubstTemplateTypeParmType const* T)
    {
        clang::QualType RT = T->getReplacementType();
        return Visit(RT);
    }

    // ----------------------------------------------------------------

    /** Visit a pack expansion type.

        This function unwraps the pattern type from the pack expansion.

        Example:
        - Wrapped type: `int...`
        - Unwrapped type: `int`
     */
    bool
    VisitPackExpansionType(
        clang::PackExpansionType const* T)
    {
        IsPack_ = true;
        clang::QualType PT = T->getPattern();
        return Visit(PT);
    }

    // ----------------------------------------------------------------

    /** Visit a pointer type.

        This function unwraps the pointee type from the pointer type.

        Example:
        - Wrapped type: `int*`
        - Unwrapped type: `int`
     */
    bool
    VisitPointerType(
        clang::PointerType const* T)
    {
        getDerived().buildPointer(T, std::exchange(Quals_, 0));
        clang::QualType PT = T->getPointeeType();
        return Visit(PT);
    }

    /** Visit an lvalue reference type.

        This function unwraps the pointee type from the lvalue reference type.

        Example:
        - Wrapped type: `int&`
        - Unwrapped type: `int`
     */
    bool
    VisitLValueReferenceType(
        clang::LValueReferenceType const* T)
    {
        getDerived().buildLValueReference(T);
        Quals_ = 0;
        clang::QualType PT = T->getPointeeType();
        return Visit(PT);
    }

    /** Visit an rvalue reference type.

        This function unwraps the pointee type from the rvalue reference type.

        Example:
        - Wrapped type: `int&&`
        - Unwrapped type: `int`
     */
    bool
    VisitRValueReferenceType(
        clang::RValueReferenceType const* T)
    {
        getDerived().buildRValueReference(T);
        Quals_ = 0;
        clang::QualType PT = T->getPointeeType();
        return Visit(PT);
    }

    /** Visit a member pointer type.

        This function unwraps the pointee type from the member pointer type.

        Example:
        - Wrapped type: `int Class::*`
        - Unwrapped type: `int`
     */
    bool
    VisitMemberPointerType(
        clang::MemberPointerType const* T)
    {
        getDerived().buildMemberPointer(T, std::exchange(Quals_, 0));
        clang::QualType PT = T->getPointeeType();
        return Visit(PT);
    }

    bool
    VisitFunctionType(
        clang::FunctionType const* T)
    {
        getDerived().populate(T);
        clang::QualType RT = T->getReturnType();
        return Visit(RT);
    }

    /** Visit an array type.

        This function unwraps the element type from the array type.

        Example:
        - Wrapped type: `int[10]`
        - Unwrapped type: `int`
     */
    bool
    VisitArrayType(
        clang::ArrayType const* T)
    {
        getDerived().buildArray(T);
        clang::QualType ET = T->getElementType();
        return Visit(ET);
    }

    // ----------------------------------------------------------------

    bool
    VisitDecltypeType(
        clang::DecltypeType const* T)
    {
        getDerived().buildDecltype(T, Quals_, IsPack_);
        return true;
    }

    bool
    VisitAutoType(
        clang::AutoType const* T)
    {
        // KRYSTIAN NOTE: we don't use isDeduced because it will
        // return true if the type is dependent
        // if the type has been deduced, use the deduced type
        getDerived().buildAuto(T, Quals_, IsPack_);
        return true;
    }

    bool
    VisitDeducedTemplateSpecializationType(
        clang::DeducedTemplateSpecializationType const* T)
    {
        // KRYSTIAN TODO: we should probably add a Type
        // to represent deduced types also stores what it
        // was deduced as.
        if (clang::QualType DT = T->getDeducedType(); !DT.isNull())
        {
            return Visit(DT);
        }
        clang::TemplateName const TN = T->getTemplateName();
        MRDOCS_ASSERT(! TN.isNull());
        clang::NamedDecl* ND = TN.getAsTemplateDecl();
        getDerived().buildTerminal(TN.getQualifier(), ND,
            std::nullopt, Quals_, IsPack_);
        return true;
    }

    bool
    VisitDependentNameType(
        clang::DependentNameType const* T)
    {
        if (auto SFINAE = getASTVisitor().extractSFINAEInfo(T))
        {
            Constraints = SFINAE->Constraints;
            return getDerived().Visit(SFINAE->Type);
        }

        getDerived().buildTerminal(T->getQualifier(), T->getIdentifier(),
            std::nullopt, Quals_, IsPack_);
        return true;
    }

    bool
    VisitDependentTemplateSpecializationType(
        clang::DependentTemplateSpecializationType const* T)
    {
        MRDOCS_SYMBOL_TRACE(T, Visitor_.context_);
        const clang::DependentTemplateStorage &S = T->getDependentTemplateName();
        getDerived().buildTerminal(S.getQualifier(), S.getName().getIdentifier(),
                                   T->template_arguments(), Quals_, IsPack_);
        return true;
    }

    // Visit a template specialization such as `A<T>`
    bool
    VisitTemplateSpecializationType(
        clang::TemplateSpecializationType const* T)
    {
        MRDOCS_SYMBOL_TRACE(T, Visitor_.context_);
        if (auto SFINAE = getASTVisitor().extractSFINAEInfo(T))
        {
            Constraints = SFINAE->Constraints;
            return getDerived().Visit(SFINAE->Type);
        }

        // In most cases, a template name is simply a reference
        // to a class template. In `X<int> xi;` the template name
        // is `template<typename T> class X { };`.
        // Template names can also refer to function templates,
        // C++0x template aliases, etc...
        clang::TemplateName const TN = T->getTemplateName();
        MRDOCS_SYMBOL_TRACE(TN, Visitor_.context_);
        MRDOCS_ASSERT(! TN.isNull());

        // The list of template parameters and a reference to
        // the templated scoped declaration
        clang::NamedDecl* D = TN.getAsTemplateDecl();
        MRDOCS_SYMBOL_TRACE(TN, Visitor_.context_);

        if (!T->isTypeAlias())
        {
            if (auto* CT = dyn_cast<clang::TagType>(T->getCanonicalTypeInternal()))
            {
                MRDOCS_SYMBOL_TRACE(CT, Visitor_.context_);
                D = CT->getOriginalDecl()->getDefinitionOrSelf();
                MRDOCS_SYMBOL_TRACE(D, Visitor_.context_);
            }
        }

        getDerived().buildTerminal(
            TN.getQualifier(), D,
            T->template_arguments(),
            Quals_, IsPack_);
        return true;
    }

    bool
    VisitRecordType(
        clang::RecordType const* T)
    {
        clang::RecordDecl* RD = T->getOriginalDecl()->getDefinitionOrSelf();
        // if this is an instantiation of a class template,
        // create a SpecializationType & extract the template arguments
        Optional<llvm::ArrayRef<clang::TemplateArgument>> TArgs = std::nullopt;
        if (auto const* CTSD = dyn_cast<clang::ClassTemplateSpecializationDecl>(RD))
        {
            TArgs = CTSD->getTemplateArgs().asArray();
        }
        getDerived().buildTerminal(T->getQualifier(), RD, TArgs, Quals_,
                                   IsPack_);
        return true;
    }

    bool
    VisitInjectedClassNameType(
        clang::InjectedClassNameType const* T)
    {
        getDerived().buildTerminal(T->getQualifier(), T->getOriginalDecl()->getDefinitionOrSelf(),
            std::nullopt, Quals_, IsPack_);
        return true;
    }

    bool
    VisitEnumType(
        clang::EnumType const* T)
    {
        getDerived().buildTerminal(T->getQualifier(), T->getOriginalDecl()->getDefinitionOrSelf(),
            std::nullopt, Quals_, IsPack_);
        return true;
    }

    bool
    VisitTypedefType(
        clang::TypedefType const* T)
    {
        getDerived().buildTerminal(T->getQualifier(), T->getDecl(),
            std::nullopt, Quals_, IsPack_);
        return true;
    }

    bool
    VisitTemplateTypeParmType(
        clang::TemplateTypeParmType const* T)
    {
        MRDOCS_SYMBOL_TRACE(T, Visitor_.context_);
        clang::IdentifierInfo const* II = nullptr;
        if (clang::TemplateTypeParmDecl const* D = T->getDecl())
        {
            MRDOCS_SYMBOL_TRACE(D, Visitor_.context_);
            if(D->isImplicit())
            {
                // special case for implicit template parameters
                // resulting from abbreviated function templates
                getDerived().buildTerminal(
                    T, Quals_, IsPack_);
                return true;
            }
            II = D->getIdentifier();
        }
        getDerived().buildTerminal(std::nullopt, II,
            std::nullopt, Quals_, IsPack_);
        return true;
    }

    bool
    VisitSubstTemplateTypeParmPackType(
        clang::SubstTemplateTypeParmPackType const* T)
    {
        getDerived().buildTerminal(std::nullopt, T->getIdentifier(),
            std::nullopt, Quals_, IsPack_);
        return true;
    }

    bool
    VisitType(clang::Type const* T)
    {
        getDerived().buildTerminal(
            T, Quals_, IsPack_);
        return true;
    }
};

} // mrdocs

#endif
