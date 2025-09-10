//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SPECIFIERS_HPP
#define MRDOCS_API_METADATA_SPECIFIERS_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <string>

namespace clang::mrdocs {

/** Access specifier.

    None is set to zero since it is the most
    frequently occurring access, and it is
    elided by the bitstream encoder because it
    has an all-zero bit pattern. This improves
    compression in the bitstream.

    None is used for namespace members and friend;
    such declarations have no access.
*/
enum class AccessKind
{
    /// Unspecified access
    None = 0,
    /// Public access
    Public,
    /// Protected access
    Protected,
    /// Private access
    Private,
};

/** `constexpr`/`consteval` specifier kinds

    [dcl.spec.general] p2: At most one of the `constexpr`, `consteval`,
    and `constinit` keywords shall appear in a decl-specifier-seq
*/
enum class ConstexprKind
{
    /// No `constexpr` or `consteval` specifier
    None = 0,
    /// The `constexpr` specifier
    Constexpr,
    /// The `consteval` specifier
    /// only valid for functions
    Consteval,
};

/** Explicit specifier kinds
*/
enum class ExplicitKind
{
    /** No explicit-specifier or explicit-specifier evaluated to false
    */
    False = 0,
    /** explicit-specifier evaluates to true
    */
    True,
    /** Dependent explicit-specifier
    */
    Dependent
};

// KRYSTIAN FIXME: this needs to be improved (a lot)
struct ExplicitInfo
{
    /** Whether an explicit-specifier was user-written.
    */
    bool Implicit = true;

    /** The evaluated exception specification.
    */
    ExplicitKind Kind = ExplicitKind::False;

    /** The operand of the explicit-specifier, if any.
    */
    std::string Operand;

    auto operator<=>(ExplicitInfo const&) const = default;
};

/** Exception specification kinds
*/
enum class NoexceptKind
{
    /** Potentially-throwing exception specification
    */
    False = 0,
    /** Non-throwing exception specification
    */
    True,
    /** Dependent exception specification
    */
    Dependent
};

// KRYSTIAN FIXME: this needs to be improved (a lot)
struct NoexceptInfo
{
    /** Whether a noexcept-specifier was user-written.
    */
    bool Implicit = true;

    /** The evaluated exception specification.
    */
    NoexceptKind Kind = NoexceptKind::False;

    /** The operand of the noexcept-specifier, if any.
    */
    std::string Operand;

    auto operator<=>(NoexceptInfo const&) const = default;
};

/** Operator kinds
*/
enum class OperatorKind
{
    /// No operator
    None = 0,
    /// The `new` Operator
    New,
    /// The `delete` Operator
    Delete,
    /// The `new[]` Operator
    ArrayNew,
    /// The `delete[]` Operator
    ArrayDelete,
    /// The + Operator
    Plus,
    /// The - Operator
    Minus,
    /// The * Operator
    Star,
    /// The / Operator
    Slash,
    /// The % Operator
    Percent,
    /// The ^ Operator
    Caret,
    /// The & Operator
    Amp,
    /// The | Operator
    Pipe,
    /// The ~ Operator
    Tilde,
    /// The ! Operator
    Equal,
    /// The += Operator
    PlusEqual,
    /// The -= Operator
    MinusEqual,
    /// The *= Operator
    StarEqual,
    /// The /= Operator
    SlashEqual,
    /// The %= Operator
    PercentEqual,
    /// The ^= Operator
    CaretEqual,
    /// The &= Operator
    AmpEqual,
    /// The |= Operator
    PipeEqual,
    /// The << Operator
    LessLess,
    /// The >> Operator
    GreaterGreater,
    /// The <<= Operator
    LessLessEqual,
    /// The >>= Operator
    GreaterGreaterEqual,

    // Relational operators
    /// The ! Operator
    Exclaim,
    /// The == Operator
    EqualEqual,
    /// The != Operator
    ExclaimEqual,
    /// The < Operator
    Less,
    /// The <= Operator
    LessEqual,
    /// The > Operator
    Greater,
    /// The >= Operator
    GreaterEqual,
    /// The <=> Operator
    Spaceship,

    /// The && Operator
    AmpAmp,
    /// The || Operator
    PipePipe,
    /// The ++ Operator
    PlusPlus,
    /// The -- Operator
    MinusMinus,
    /// The , Operator
    Comma,
    /// The ->* Operator
    ArrowStar,
    /// The -> Operator
    Arrow,
    /// The () Operator
    Call,
    /// The [] Operator
    Subscript,
    /// The `? :` Operator
    Conditional,
    /// The `coawait` Operator
    Coawait,
};

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    OperatorKind kind)
{
    v = static_cast<std::underlying_type_t<OperatorKind>>(kind);
}

/** Determines whether the operator is potentially unary.
 */
MRDOCS_DECL
bool
isUnaryOperator(OperatorKind kind) noexcept;

/** Determines whether the operator is potentially binary.
 */
MRDOCS_DECL
bool
isBinaryOperator(OperatorKind kind) noexcept;

/** Reference type kinds
*/
enum class ReferenceKind
{
    /// Not a reference
    None = 0,
    /// An L-Value reference
    LValue,
    /// An R-Value reference
    RValue
};

/** Storage class kinds

    [dcl.stc] p1: At most one storage-class-specifier shall appear
    in a given decl-specifier-seq, except that `thread_local`
    may appear with `static` or `extern`.
*/
enum class StorageClassKind
{
    /// No storage class specifier
    None = 0,
    /// thread_local storage-class-specifier
    Extern,
    /// mutable storage-class-specifier
    Static,
    /// auto storage-class-specifier (removed in C++11)
    /// only valid for variables
    Auto,
    /// register storage-class-specifier (removed in C++17)
    /// only valid for variables
    Register
};

MRDOCS_DECL dom::String toString(AccessKind kind) noexcept;
MRDOCS_DECL dom::String toString(ConstexprKind kind) noexcept;
MRDOCS_DECL dom::String toString(ExplicitKind kind) noexcept;
MRDOCS_DECL dom::String toString(NoexceptKind kind) noexcept;
MRDOCS_DECL dom::String toString(ReferenceKind kind) noexcept;
MRDOCS_DECL dom::String toString(StorageClassKind kind) noexcept;

/** Convert NoexceptInfo to a string.

    @param info The noexcept-specifier information.

    @param resolved If true, the operand is not shown when
    the exception specification is non-dependent.

    @param implicit If true, implicit exception specifications
    are shown.

    @return The string representation of the noexcept-specifier.
*/
MRDOCS_DECL
dom::String
toString(
    NoexceptInfo const& info,
    bool resolved = false,
    bool implicit = false);

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    NoexceptInfo const& info)
{
    v = toString(info, false, false);
}

/** Convert ExplicitInfo to a string.

    @param info The explicit-specifier information.
    @param resolved If true, the operand is not shown when
    the explicit-specifier is non-dependent.
    @param implicit If true, implicit explicit-specifiers are shown.
    @return The string representation of the explicit-specifier.
*/
MRDOCS_DECL
dom::String
toString(
    ExplicitInfo const& info,
    bool resolved = false,
    bool implicit = false);

/** Return the ExplicitInfo as a @ref dom::Value string.

    @param v The output parameter to receive the dom::Value.
    @param I The ExplicitInfo to convert.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    ExplicitInfo const& I)
{
    v = toString(I);
}

/** Return the AccessKind as a @ref dom::Value string.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    AccessKind const kind)
{
    v = toString(kind);
}

/** Return the ConstexprKind as a @ref dom::Value string.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    ConstexprKind const kind)
{
    v = toString(kind);
}

/** Return the StorageClassKind as a @ref dom::Value string.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    StorageClassKind const kind)
{
    v = toString(kind);
}

/** Return the ReferenceKind as a @ref dom::Value string.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    ReferenceKind kind)
{
    v = toString(kind);
}

} // clang::mrdocs

#endif
