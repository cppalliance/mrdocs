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
#include <string_view>

namespace clang {
namespace mrdocs {

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
    // no explicit-specifier present
    None = 0,
    // explicit-specifier, no constant-expression
    Explicit,
    // explicit-specifier, constant-expression evaluates to false
    ExplicitFalse,
    // explicit-specifier, constant-expression evaluates to true
    ExplicitTrue,
    // explicit-specifier, unresolved
    ExplicitUnresolved
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
    Exclaim,
    Equal,
    Less,
    Greater,
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
    EqualEqual,
    ExclaimEqual,
    LessEqual,
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
    NoexceptInfo info,
    bool resolved = false,
    bool implicit = false);

} // mrdocs
} // clang

#endif
