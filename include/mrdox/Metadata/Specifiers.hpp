//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_METADATA_SPECIFIERS_HPP
#define MRDOX_API_METADATA_SPECIFIERS_HPP

#include <mrdox/Platform.hpp>
#include <string_view>

namespace clang {
namespace mrdox {

/** Access specifier.

    Public is set to zero since it is the most
    frequently occurring access, and it is
    elided by the bitstream encoder because it
    has an all-zero bit pattern. This improves
    compression in the bitstream.

    None is used for namespace members and friend;
    such declarations have no access.
*/
enum class AccessKind
{
    Public = 0,
    Protected,
    Private,
    None
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

/** `constexpr`/`consteval`/`constinit` specifier kinds

    [dcl.spec.general] p2: At most one of the `constexpr`, `consteval`,
    and `constinit` keywords shall appear in a decl-specifier-seq
*/
enum class ConstexprKind
{
    None = 0,
    Constexpr,
    // only valid for functions
    Consteval,
    // only valid for variables
    Constinit
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
    None = 0,
    // throw()
    ThrowNone,
    // throw(type-id-list)
    Throw,
    // throw(...) (microsoft extension)
    ThrowAny,
    // __declspec(nothrow) (microsoft extension)
    NoThrow,
    // noexcept-specifier, no constant-expression
    Noexcept,
    // noexcept-specifier, constant-expression evaluates to false
    NoexceptFalse,
    // noexcept-specifier, constant-expression evaluates to true
    NoexceptTrue,
    // noexcept-specifier, dependent constant-expression
    NoexceptDependent,

    // not evaluated yet, for special member function
    Unevaluated,
    // not instantiated yet
    Uninstantiated,
    // not parsed yet
    Unparsed
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

MRDOX_DECL
std::string_view
toString(AccessKind kind);

MRDOX_DECL
std::string_view
toString(StorageClassKind kind);

MRDOX_DECL
std::string_view
toString(ConstexprKind kind);

MRDOX_DECL
std::string_view
toString(ExplicitKind kind);

MRDOX_DECL
std::string_view
toString(NoexceptKind kind);

MRDOX_DECL
std::string_view
toString(ReferenceKind kind);

} // mrdox
} // clang

#endif
