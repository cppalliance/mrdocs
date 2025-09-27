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

#ifndef MRDOCS_LIB_AST_TYPEBUILDER_HPP
#define MRDOCS_LIB_AST_TYPEBUILDER_HPP

#include <lib/AST/TerminalTypeVisitor.hpp>

namespace mrdocs {

/** A visitor to build a `mrdocs::Type` from a `clang::Type`.

    This class is used to build type information by visiting
    various terminal types. The method `Visit` from the
    base class can be used to iterate over the type information
    and build the corresponding `Type` object:

    @code
    TypeBuilder Builder(astVisitor);
    Builder.Visit(qt);
    std::unique_ptr<Type> typeInfo = Builder.result();
    @endcode
 */
class TypeBuilder
    : public TerminalTypeVisitor<TypeBuilder>
{
    /* The resulting of converting a QualType to a Type

       This variable holds the result of the type information
       as a polymorphic `Type` object.

     */
    Polymorphic<Type> Result = Polymorphic<Type>(AutoType{});

    /*  A pointer to the inner type of result currently being populated.

        The Result variable is a polymorphic `Type` object that
        might contain nested type information, also represented
        as a `Type` object.

        For instance `int&` is represented as a `ReferenceType`
        object that contains a `NamedType` object representing
        the `int` type.

        The builder will always populate the inner type of the
        result being constructed. For instance, when building
        a `ReferenceType` object for `int&`, the inner type
        (initially the same as the result) will be set to a
        `LValueReferenceType`, that contains the `NamedType`
        as a member. So `Inner` becomes a pointer to this
        `NamedType` object, and the visiting process continues
        populating the `Inner` object.
     */
    Polymorphic<Type>* Inner = &Result;

public:
    using TerminalTypeVisitor::TerminalTypeVisitor;

    /** Get the result of the type information.

        This function returns the result of the type information
        as a unique pointer to the `Type` object.

        @return A unique pointer to the `Type` object.
     */
    Polymorphic<Type>
    result()
    {
        return std::move(Result);
    }

    /** Build type information for a pointer type.

        Create a `PointerType` object and populate it with
        the qualifiers and the pointee type.

        @param T The pointer type to build.
        @param quals The qualifiers for the pointer type.
     */
    void buildPointer(clang::PointerType const* T, unsigned quals);

    /** Build type information for a lvalue reference type.

        Create a `LValueReferenceType` object and populate it with
        the pointee type.

        @param T The lvalue reference type to build.
     */
    void buildLValueReference(clang::LValueReferenceType const* T);

    /** Build type information for an rvalue reference type.

        Create a `RValueReferenceType` object and populate it with
        the pointee type.

        @param T The rvalue reference type to build.
     */
    void buildRValueReference(clang::RValueReferenceType const* T);

    /** Build type information for a member pointer type.

        Create a `MemberPointerType` object and populate it with
        the qualifiers and the parent type.

        A `MemberPointerType` object is used to represent a pointer
        to a member of a class.

        @param T The member pointer type to build.
        @param quals The qualifiers for the member pointer type.
     */
    void buildMemberPointer(clang::MemberPointerType const* T, unsigned quals);

    /** Build type information for an array type.

        Create an `ArrayType` object and populate it with the
        element type and the array bounds.

        An `ArrayType` object is used to represent an array type.
        It includes the internal `Type` object for the element type
        and the expression defining the array bounds.

        @param T The array type to build.
     */
    void buildArray(clang::ArrayType const* T);

    /** Populate type information for a function type.

        Create a `FunctionType` object and populate it with
        the function type information.

        A `FunctionType` object is used to represent a function type.
        It includes the return type and the parameter types.

        @param T The function type to populate.
     */
    void populate(clang::FunctionType const* T);

    /** Build type information for a decltype type.

        Create a `DecltypeType` object and populate it with
        the underlying expression.

        A `DecltypeType` object is used to represent a decltype type.
        It includes the underlying expression.

        @param T The decltype type to build.
        @param quals The qualifiers for the decltype type.
        @param pack Whether the decltype type is a pack.
     */
    void buildDecltype(clang::DecltypeType const* T, unsigned quals, bool pack);

    /** Build type information for an auto type.

        Create an `AutoType` object and populate it with
        the qualifiers.

        An `AutoType` object is used to represent an auto type.
        It includes the qualifiers for the auto type, the keyword
        used to declare the auto type, and constraints.

        @param T The auto type to build.
        @param quals The qualifiers for the auto type.
        @param pack Whether the auto type is a pack.
     */
    void buildAuto(clang::AutoType const* T, unsigned quals, bool pack);

    /** Build type information for a terminal type.

        Create a `NamedType` object and populate it with
        the name information.

        A `NamedType` object is used to represent a terminal type.
        It includes the name information, the nested name specifier,
        and the qualifiers for the terminal type.

        @param NNS The nested name specifier.
        @param T The terminal type to build.
        @param quals The qualifiers for the terminal type.
        @param pack Whether the terminal type is a pack.
     */
    void buildTerminal(
        clang::Type const* T,
        unsigned quals,
        bool pack);

    /** Build type information for a terminal type with an identifier.

        Create a `NamedType` object and populate it with
        the name information.

        A `NamedType` object is used to represent a terminal type.
        It includes the name information, the nested name specifier,
        and the qualifiers for the terminal type.

        @param NNS The nested name specifier.
        @param II The identifier info.
        @param TArgs The template arguments.
        @param quals The qualifiers for the terminal type.
        @param pack Whether the terminal type is a pack.
     */
    void buildTerminal(
        clang::NestedNameSpecifier NNS,
        clang::IdentifierInfo const* II,
        Optional<llvm::ArrayRef<clang::TemplateArgument>> TArgs,
        unsigned quals,
        bool pack);

    /** Build type information for a terminal type with a named declaration.

        Create a `NamedType` object and populate it with
        the name information.

        A `NamedType` object is used to represent a terminal type.
        It includes the name information, the nested name specifier,
        and the qualifiers for the terminal type.

        @param NNS The nested name specifier.
        @param D The named declaration.
        @param TArgs The template arguments.
        @param quals The qualifiers for the terminal type.
        @param pack Whether the terminal type is a pack.
     */
    void buildTerminal(
        clang::NestedNameSpecifier NNS,
        clang::NamedDecl* D,
        Optional<llvm::ArrayRef<clang::TemplateArgument>> TArgs,
        unsigned quals,
        bool pack);
};

} // mrdocs

#endif // MRDOCS_LIB_AST_TYPEBUILDER_HPP
