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
    None = 0,
    Public,
    Protected,
    Private,
};

/** `constexpr`/`consteval` specifier kinds

    [dcl.spec.general] p2: At most one of the `constexpr`, `consteval`,
    and `constinit` keywords shall appear in a decl-specifier-seq
*/
enum class ConstexprKind
{
    None = 0,
    Constexpr,
    // only valid for functions
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

    auto operator<=>(const ExplicitInfo&) const = default;
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
    None = 0,
    New,
    Delete,
    ArrayNew,
    ArrayDelete,
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Caret,
    Amp,
    Pipe,
    Tilde,
    Equal,
    PlusEqual,
    MinusEqual,
    StarEqual,
    SlashEqual,
    PercentEqual,
    CaretEqual,
    AmpEqual,
    PipeEqual,
    LessLess,
    GreaterGreater,
    LessLessEqual,
    GreaterGreaterEqual,

    // Relational operators
    Exclaim,
    EqualEqual,
    ExclaimEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    Spaceship,

    AmpAmp,
    PipePipe,
    PlusPlus,
    MinusMinus,
    Comma,
    ArrowStar,
    Arrow,
    Call,
    Subscript,
    Conditional,
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

/** Reference type kinds
*/
enum class ReferenceKind
{
    None = 0,
    LValue,
    RValue
};

/** Storage class kinds

    [dcl.stc] p1: At most one storage-class-specifier shall appear
    in a given decl-specifier-seq, except that `thread_local`
    may appear with `static` or `extern`.
*/
enum class StorageClassKind
{
    None = 0,
    Extern,
    Static,
    // auto storage-class-specifier (removed in C++11)
    // only valid for variables
    Auto,
    // register storage-class-specifier (removed in C++17)
    // only valid for variables
    Register
};

MRDOCS_DECL dom::String toString(AccessKind kind) noexcept;
MRDOCS_DECL dom::String toString(ConstexprKind kind) noexcept;
MRDOCS_DECL dom::String toString(ExplicitKind kind) noexcept;
MRDOCS_DECL dom::String toString(NoexceptKind kind) noexcept;
MRDOCS_DECL dom::String toString(ReferenceKind kind) noexcept;
MRDOCS_DECL dom::String toString(StorageClassKind kind) noexcept;

/** Convert NoexceptInfo to a string.

    @param resolved If true, the operand is not shown when
    the exception specification is non-dependent.

    @param implicit If true, implicit exception specifications
    are shown.
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
*/
MRDOCS_DECL
dom::String
toString(
    ExplicitInfo const& info,
    bool resolved = false,
    bool implicit = false);

/** Return the ExplicitInfo as a @ref dom::Value string.
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
