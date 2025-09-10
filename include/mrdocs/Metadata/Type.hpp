//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TYPE_HPP
#define MRDOCS_API_METADATA_TYPE_HPP

#include <string>
#include <vector>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Metadata/Specifiers.hpp>
#include <mrdocs/Metadata/SymbolID.hpp>
#include <mrdocs/Platform.hpp>
#include <mrdocs/Support/TypeTraits.hpp>

namespace clang::mrdocs {

struct NameInfo;
struct TypeInfo;

std::strong_ordering
operator<=>(Polymorphic<NameInfo> const& lhs, Polymorphic<NameInfo> const& rhs);

std::strong_ordering
operator<=>(Polymorphic<TypeInfo> const& lhs, Polymorphic<TypeInfo> const& rhs);

/** Type qualifiers
*/
enum QualifierKind
{
    /// No qualifiers
    None,
    /// The const qualifier
    Const,
    /// The volatile qualifier
    Volatile
};

MRDOCS_DECL dom::String toString(QualifierKind kind) noexcept;

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    QualifierKind kind)
{
    v = toString(kind);
}

enum class TypeKind
{
    /// A Named type
    Named = 1, // for bitstream
    /// A decltype type
    Decltype,
    /// An auto type
    Auto,
    /// An LValueReference type
    LValueReference,
    /// An RValueReference type
    RValueReference,
    /// A Pointer type
    Pointer,
    /// A MemberPointer type
    MemberPointer,
    /// An Array type
    Array,
    /// A Function type
    Function,
};

MRDOCS_DECL dom::String toString(TypeKind kind) noexcept;

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    TypeKind kind)
{
    v = toString(kind);
}

/** The kind of `auto` keyword used in a declaration.

    This is either `auto` or `decltype(auto)`.
*/
enum class AutoKind
{
    /// The `auto` keyword
    Auto,
    /// The `decltype(auto)` keyword
    DecltypeAuto
};

MRDOCS_DECL dom::String toString(AutoKind kind) noexcept;

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    AutoKind kind)
{
    v = toString(kind);
}

/** A possibly qualified type.

    This class represents a type that may have
    qualifiers (e.g. const, volatile).

    This base class is used to store the kind
    of type. Derived classes are used to store
    the type information according to the kind.
 */
struct TypeInfo
{
    /** The kind of TypeInfo this is
    */
    TypeKind Kind;

    /** Whether this is the pattern of a pack expansion.
    */
    bool IsPackExpansion = false;

    /** The const qualifier
     */
    bool IsConst = false;

    /** The volatile qualifier
     */
    bool IsVolatile = false;

    /** The constraints associated with the type

        This represents the constraints associated with the type,
        such as SFINAE constraints.

        For instance, if SFINAE detection is enabled, the
        expression `std::enable_if_t<std::is_integral_v<T>, T>`
        will have type `T` (NamedType) and constraints
        `{std::is_integral_v<T>}`.
     */
    std::vector<ExprInfo> Constraints;

    constexpr virtual ~TypeInfo() = default;

    constexpr bool isNamed()           const noexcept { return Kind == TypeKind::Named; }
    constexpr bool isDecltype()        const noexcept { return Kind == TypeKind::Decltype; }
    constexpr bool isAuto()            const noexcept { return Kind == TypeKind::Auto; }
    constexpr bool isLValueReference() const noexcept { return Kind == TypeKind::LValueReference; }
    constexpr bool isRValueReference() const noexcept { return Kind == TypeKind::RValueReference; }
    constexpr bool isPointer()         const noexcept { return Kind == TypeKind::Pointer; }
    constexpr bool isMemberPointer()   const noexcept { return Kind == TypeKind::MemberPointer; }
    constexpr bool isArray()           const noexcept { return Kind == TypeKind::Array; }
    constexpr bool isFunction()        const noexcept { return Kind == TypeKind::Function; }

    /** Return the symbol named by this type.
    */
    SymbolID
    namedSymbol() const noexcept;

    auto operator<=>(TypeInfo const&) const = default;

protected:
    constexpr
    TypeInfo(
        TypeKind kind) noexcept
        : Kind(kind)
    {
    }
};

MRDOCS_DECL
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    TypeInfo const& I,
    DomCorpus const* domCorpus);

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Polymorphic<TypeInfo> const& I,
    DomCorpus const* domCorpus)
{
    if (!I)
    {
        v = nullptr;
        return;
    }
    tag_invoke(dom::ValueFromTag{}, v, *I, domCorpus);
}

template<TypeKind K>
struct TypeInfoCommonBase : TypeInfo
{
    static constexpr TypeKind kind_id = K;

    static constexpr bool isNamed()           noexcept { return K == TypeKind::Named; }
    static constexpr bool isDecltype()        noexcept { return K == TypeKind::Decltype; }
    static constexpr bool isAuto()            noexcept { return K == TypeKind::Auto; }
    static constexpr bool isLValueReference() noexcept { return K == TypeKind::LValueReference; }
    static constexpr bool isRValueReference() noexcept { return K == TypeKind::RValueReference; }
    static constexpr bool isPointer()         noexcept { return K == TypeKind::Pointer; }
    static constexpr bool isMemberPointer()   noexcept { return K == TypeKind::MemberPointer; }
    static constexpr bool isArray()           noexcept { return K == TypeKind::Array; }
    static constexpr bool isFunction()        noexcept { return K == TypeKind::Function; }

    auto operator<=>(TypeInfoCommonBase const&) const = default;

protected:
    constexpr
    TypeInfoCommonBase() noexcept
        : TypeInfo(K)
    {
    }
};

/** Categorically describes a fundamental type.

    @see https://en.cppreference.com/w/cpp/language/types
*/
enum class FundamentalTypeKind
{
    /// void
    Void,
    /// std::nullptr_t
    Nullptr,
    /// bool
    Bool,
    /// char
    Char,
    /// signed char
    SignedChar,
    /// unsigned char
    UnsignedChar,
    /// char8_t
    Char8,
    /// char16_t
    Char16,
    /// char32_t
    Char32,
    /// wchar_t
    WChar,
    /// short / short int / signed short / signed short int
    Short,
    /// unsigned short / unsigned short int
    UnsignedShort,
    /// int / signed / signed int
    Int,
    /// unsigned / unsigned int
    UnsignedInt,
    /// long / long int / signed long / signed long int
    Long,
    /// unsigned long / unsigned long int
    UnsignedLong,
    /// long long / long long int / signed long long / signed long long int
    LongLong,
    /// unsigned long long / unsigned long long int
    UnsignedLongLong,
    /// float
    Float,
    /// double
    Double,
    /// long double
    LongDouble
};

/** Convert a FundamentalTypeKind to a string.

    This function converts a FundamentalTypeKind to
    the shortest canonical string representing the type.

    @return The string representation of the kind
 */
MRDOCS_DECL
std::string_view
toString(FundamentalTypeKind kind) noexcept;

/** Convert a string to a FundamentalTypeKind.

    This function converts a string to a FundamentalTypeKind.

    All variations of the type specifiers are supported.

    However, the "long long" specifier cannot be split
    into two separate specifiers.

    @param str The string to convert
    @param[out] kind The resulting FundamentalTypeKind

    @return true if the string was successfully converted
 */
MRDOCS_DECL
bool
fromString(std::string_view str, FundamentalTypeKind& kind) noexcept;

/** Apply the "long" specifier to the type

    If applying "long" the specifier is a valid operation
    the function changes the type and returns true.

    For instance, applying "long" to
    `FundamentalTypeKind::Int` ("int") results in
    `FundamentalTypeKind::Long` ("long int").

    @param[in/out] kind The type to modify
    @return Whether the operation was successful
 */
MRDOCS_DECL
bool
makeLong(FundamentalTypeKind& kind) noexcept;

/** Apply the "short" specifier to the type

    If applying "short" the specifier is a valid operation
    the function changes the type and returns true.

    For instance, applying "short" to
    `FundamentalTypeKind::Int` ("int") results in
    `FundamentalTypeKind::Short` ("short int").

    @param[in/out] kind The type to modify
    @return Whether the operation was successful
 */
MRDOCS_DECL
bool
makeShort(FundamentalTypeKind& kind) noexcept;

/** Apply the "signed" specifier to the type

    If applying the "signed" specifier is a valid operation
    the function changes the type and returns true.

    For instance, applying "signed" to
    `FundamentalTypeKind::Char` ("char") results in
    `FundamentalTypeKind::SignedChar` ("signed char").

    It also returns true if applying the "signed" specifier
    is a valid operation but doesn't affect the
    type.

    For instance, applying "signed" to
    `FundamentalTypeKind::Int` ("int") doesn't change the type
    but returns `true`, even though `FundamentalTypeKind::Int`
    could be declared as "int" or "signed" and multiple
    "signed" specifiers are not allowed.

    @param[in/out] kind The type to modify
    @return Whether the operation was successful
 */
MRDOCS_DECL
bool
makeSigned(FundamentalTypeKind& kind) noexcept;

/** Apply the "unsigned" specifier to the type

    If applying the "unsigned" specifier is a valid operation
    the function changes the type and returns true.

    For instance, applying "unsigned" to
    `FundamentalTypeKind::Char` ("char") results in
    `FundamentalTypeKind::UnsignedChar` ("unsigned char")
    and applying "unsigned" to
    `FundamentalTypeKind::Int` ("int") results in
    `FundamentalTypeKind::UnsignedInt` ("unsigned int").

    @param[in/out] kind The type to modify
    @return Whether the operation was successful
 */
MRDOCS_DECL
bool
makeUnsigned(FundamentalTypeKind& kind) noexcept;

/** Apply the "char" specifier to the type

    If applying the "char" specifier to a type
    that might have been declared only with "signed/unsigned"
    or "short/long" specifiers, the function changes the type
    and returns true.

    For instance, applying "char" to
    `FundamentalTypeKind::Int` ("int", which could be declared
    as "signed") results in
    `FundamentalTypeKind::SignedChar` ("signed char").

    @param[in/out] kind The type to modify
    @return Whether the operation was successful
 */
MRDOCS_DECL
bool
makeChar(FundamentalTypeKind& kind) noexcept;

struct NamedTypeInfo final
    : TypeInfoCommonBase<TypeKind::Named>
{
    Polymorphic<NameInfo> Name = std::nullopt;

    std::optional<FundamentalTypeKind> FundamentalType;

    std::strong_ordering
    operator<=>(NamedTypeInfo const& other) const;
};

struct DecltypeTypeInfo final
    : TypeInfoCommonBase<TypeKind::Decltype>
{
    ExprInfo Operand;

    auto operator<=>(DecltypeTypeInfo const&) const = default;
};

struct AutoTypeInfo final
    : TypeInfoCommonBase<TypeKind::Auto>
{
    AutoKind Keyword = AutoKind::Auto;
    Polymorphic<NameInfo> Constraint = std::nullopt;

    std::strong_ordering
    operator<=>(AutoTypeInfo const&) const;
};

struct LValueReferenceTypeInfo final
    : TypeInfoCommonBase<TypeKind::LValueReference>
{
    Polymorphic<TypeInfo> PointeeType = std::nullopt;

    std::strong_ordering
    operator<=>(LValueReferenceTypeInfo const&) const;
};

struct RValueReferenceTypeInfo final
    : TypeInfoCommonBase<TypeKind::RValueReference>
{
    Polymorphic<TypeInfo> PointeeType = std::nullopt;

    std::strong_ordering
    operator<=>(RValueReferenceTypeInfo const&) const;
};

struct PointerTypeInfo final
    : TypeInfoCommonBase<TypeKind::Pointer>
{
    Polymorphic<TypeInfo> PointeeType = std::nullopt;

    std::strong_ordering
    operator<=>(PointerTypeInfo const&) const;
};

struct MemberPointerTypeInfo final
    : TypeInfoCommonBase<TypeKind::MemberPointer>
{
    Polymorphic<TypeInfo> ParentType = std::nullopt;
    Polymorphic<TypeInfo> PointeeType = std::nullopt;

    std::strong_ordering
    operator<=>(MemberPointerTypeInfo const&) const;
};

struct ArrayTypeInfo final
    : TypeInfoCommonBase<TypeKind::Array>
{
    Polymorphic<TypeInfo> ElementType = std::nullopt;
    ConstantExprInfo<std::uint64_t> Bounds;

    std::strong_ordering
    operator<=>(ArrayTypeInfo const&) const;
};

struct FunctionTypeInfo final
    : TypeInfoCommonBase<TypeKind::Function>
{
    Polymorphic<TypeInfo> ReturnType = std::nullopt;
    std::vector<Polymorphic<TypeInfo>> ParamTypes;
    ReferenceKind RefQualifier = ReferenceKind::None;
    NoexceptInfo ExceptionSpec;
    bool IsVariadic = false;

    std::strong_ordering
    operator<=>(FunctionTypeInfo const&) const;
};

template<
    std::derived_from<TypeInfo> TypeTy,
    class F,
    class... Args>
decltype(auto)
visit(
    TypeTy& I,
    F&& f,
    Args&&... args)
{
    add_cv_from_t<TypeTy, TypeInfo>& II = I;
    switch(I.Kind)
    {
    case TypeKind::Named:
        return f(static_cast<add_cv_from_t<
            TypeTy, NamedTypeInfo>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::Decltype:
        return f(static_cast<add_cv_from_t<
            TypeTy, DecltypeTypeInfo>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::Auto:
        return f(static_cast<add_cv_from_t<
            TypeTy, AutoTypeInfo>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::LValueReference:
        return f(static_cast<add_cv_from_t<
            TypeTy, LValueReferenceTypeInfo>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::RValueReference:
        return f(static_cast<add_cv_from_t<
            TypeTy, RValueReferenceTypeInfo>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::Pointer:
        return f(static_cast<add_cv_from_t<
            TypeTy, PointerTypeInfo>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::MemberPointer:
        return f(static_cast<add_cv_from_t<
            TypeTy, MemberPointerTypeInfo>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::Array:
        return f(static_cast<add_cv_from_t<
            TypeTy, ArrayTypeInfo>&>(II),
                std::forward<Args>(args)...);
    case TypeKind::Function:
        return f(static_cast<add_cv_from_t<
            TypeTy, FunctionTypeInfo>&>(II),
                std::forward<Args>(args)...);
    default:
        MRDOCS_UNREACHABLE();
    }
}

MRDOCS_DECL
std::strong_ordering
operator<=>(Polymorphic<TypeInfo> const& lhs, Polymorphic<TypeInfo> const& rhs);

inline
bool
operator==(Polymorphic<TypeInfo> const& lhs, Polymorphic<TypeInfo> const& rhs) {
    return lhs <=> rhs == std::strong_ordering::equal;
}

/** Return the inner type.

    The inner type is the type which is modified
    by a specifier (e.g. "int" in "pointer to int".
*/
MRDOCS_DECL
std::optional<std::reference_wrapper<Polymorphic<TypeInfo> const>>
innerType(TypeInfo const& TI) noexcept;

/// @copydoc innerType(TypeInfo const&)
MRDOCS_DECL
std::optional<std::reference_wrapper<Polymorphic<TypeInfo>>>
innerType(TypeInfo& TI) noexcept;

/// @copydoc innerType(TypeInfo const&)
MRDOCS_DECL
TypeInfo const*
innerTypePtr(TypeInfo const& TI) noexcept;

/// @copydoc innerTypePtr(TypeInfo const&)
MRDOCS_DECL
TypeInfo*
innerTypePtr(TypeInfo& TI) noexcept;

/** Return the innermost type.

    The innermost type is the type which is not
    modified by any specifiers (e.g. "int" in
    "pointer to const int").

    If the type has an inner type, we recursively
    call this function until we reach the innermost
    type. If the type has no inner type, we return
    the current type.
*/
MRDOCS_DECL
Polymorphic<TypeInfo> const&
innermostType(Polymorphic<TypeInfo> const& TI) noexcept;

/// @copydoc innermostType(Polymorphic<TypeInfo> const&)
MRDOCS_DECL
Polymorphic<TypeInfo>&
innermostType(Polymorphic<TypeInfo>& TI) noexcept;

// VFALCO maybe we should rename this to `renderType` or something?
MRDOCS_DECL
std::string
toString(
    TypeInfo const& T,
    std::string_view Name = "");

} // clang::mrdocs

#endif
