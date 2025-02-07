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

#ifndef MRDOCS_LIB_AST_TYPEINFOBUILDER_HPP
#define MRDOCS_LIB_AST_TYPEINFOBUILDER_HPP

#include <lib/AST/TerminalTypeVisitor.hpp>

namespace clang::mrdocs {

/** A visitor to build a `mrdocs::TypeInfo` from a `clang::Type`.

    This class is used to build type information by visiting
    various terminal types. The method `Visit` from the
    base class can be used to iterate over the type information
    and build the corresponding `TypeInfo` object:

    @code
    TypeInfoBuilder Builder(astVisitor);
    Builder.Visit(qt);
    std::unique_ptr<TypeInfo> typeInfo = Builder.result();
    @endcode
 */
class TypeInfoBuilder
    : public TerminalTypeVisitor<TypeInfoBuilder>
{
    /* The resulting of converting a QualType to a TypeInfo

       This variable holds the result of the type information
       as a polymorphic `TypeInfo` object.

     */
    Polymorphic<TypeInfo> Result;

    /*  A pointer to the inner type of result currently being populated.

        The Result variable is a polymorphic `TypeInfo` object that
        might contain nested type information, also represented
        as a `TypeInfo` object.

        For instance `int&` is represented as a `ReferenceTypeInfo`
        object that contains a `NamedTypeInfo` object representing
        the `int` type.

        The builder will always populate the inner type of the
        result being constructed. For instance, when building
        a `ReferenceTypeInfo` object for `int&`, the inner type
        (initially the same as the result) will be set to a
        `LValueReferenceTypeInfo`, that contains the `NamedTypeInfo`
        as a member. So `Inner` becomes a pointer to this
        `NamedTypeInfo` object, and the visiting process continues
        populating the `Inner` object.
     */
    Polymorphic<TypeInfo>* Inner = &Result;

public:
    using TerminalTypeVisitor::TerminalTypeVisitor;

    /** Get the result of the type information.

        This function returns the result of the type information
        as a unique pointer to the `TypeInfo` object.

        @return A unique pointer to the `TypeInfo` object.
     */
    Polymorphic<TypeInfo>
    result()
    {
        return std::move(Result);
    }

    /** Build type information for a pointer type.

        Create a `PointerTypeInfo` object and populate it with
        the qualifiers and the pointee type.

        @param T The pointer type to build.
        @param quals The qualifiers for the pointer type.
     */
    void buildPointer(PointerType const* T, unsigned quals);

    /** Build type information for a lvalue reference type.

        Create a `LValueReferenceTypeInfo` object and populate it with
        the pointee type.

        @param T The lvalue reference type to build.
     */
    void buildLValueReference(LValueReferenceType const* T);

    /** Build type information for an rvalue reference type.

        Create a `RValueReferenceTypeInfo` object and populate it with
        the pointee type.

        @param T The rvalue reference type to build.
     */
    void buildRValueReference(RValueReferenceType const* T);

    /** Build type information for a member pointer type.

        Create a `MemberPointerTypeInfo` object and populate it with
        the qualifiers and the parent type.

        A `MemberPointerTypeInfo` object is used to represent a pointer
        to a member of a class.

        @param T The member pointer type to build.
        @param quals The qualifiers for the member pointer type.
     */
    void buildMemberPointer(MemberPointerType const* T, unsigned quals);

    /** Build type information for an array type.

        Create an `ArrayTypeInfo` object and populate it with the
        element type and the array bounds.

        An `ArrayTypeInfo` object is used to represent an array type.
        It includes the internal `TypeInfo` object for the element type
        and the expression defining the array bounds.

        @param T The array type to build.
     */
    void buildArray(ArrayType const* T);

    /** Populate type information for a function type.

        Create a `FunctionTypeInfo` object and populate it with
        the function type information.

        A `FunctionTypeInfo` object is used to represent a function type.
        It includes the return type and the parameter types.

        @param T The function type to populate.
     */
    void populate(FunctionType const* T);

    /** Build type information for a decltype type.

        Create a `DecltypeTypeInfo` object and populate it with
        the underlying expression.

        A `DecltypeTypeInfo` object is used to represent a decltype type.
        It includes the underlying expression.

        @param T The decltype type to build.
        @param quals The qualifiers for the decltype type.
        @param pack Whether the decltype type is a pack.
     */
    void buildDecltype(DecltypeType const* T, unsigned quals, bool pack);

    /** Build type information for an auto type.

        Create an `AutoTypeInfo` object and populate it with
        the qualifiers.

        An `AutoTypeInfo` object is used to represent an auto type.
        It includes the qualifiers for the auto type, the keyword
        used to declare the auto type, and constraints.

        @param T The auto type to build.
        @param quals The qualifiers for the auto type.
        @param pack Whether the auto type is a pack.
     */
    void buildAuto(AutoType const* T, unsigned quals, bool pack);

    /** Build type information for a terminal type.

        Create a `NamedTypeInfo` object and populate it with
        the name information.

        A `NamedTypeInfo` object is used to represent a terminal type.
        It includes the name information, the nested name specifier,
        and the qualifiers for the terminal type.

        @param NNS The nested name specifier.
        @param T The terminal type to build.
        @param quals The qualifiers for the terminal type.
        @param pack Whether the terminal type is a pack.
     */
    void buildTerminal(
        NestedNameSpecifier const* NNS,
        Type const* T,
        unsigned quals,
        bool pack);

    /** Build type information for a terminal type with an identifier.

        Create a `NamedTypeInfo` object and populate it with
        the name information.

        A `NamedTypeInfo` object is used to represent a terminal type.
        It includes the name information, the nested name specifier,
        and the qualifiers for the terminal type.

        @param NNS The nested name specifier.
        @param II The identifier info.
        @param TArgs The template arguments.
        @param quals The qualifiers for the terminal type.
        @param pack Whether the terminal type is a pack.
     */
    void buildTerminal(
        NestedNameSpecifier const* NNS,
        IdentifierInfo const* II,
        std::optional<ArrayRef<TemplateArgument>> TArgs,
        unsigned quals,
        bool pack);

    /** Build type information for a terminal type with a named declaration.

        Create a `NamedTypeInfo` object and populate it with
        the name information.

        A `NamedTypeInfo` object is used to represent a terminal type.
        It includes the name information, the nested name specifier,
        and the qualifiers for the terminal type.

        @param NNS The nested name specifier.
        @param D The named declaration.
        @param TArgs The template arguments.
        @param quals The qualifiers for the terminal type.
        @param pack Whether the terminal type is a pack.
     */
    void buildTerminal(
        NestedNameSpecifier const* NNS,
        NamedDecl* D,
        std::optional<ArrayRef<TemplateArgument>> TArgs,
        unsigned quals,
        bool pack);
};

} // clang::mrdocs

#endif
