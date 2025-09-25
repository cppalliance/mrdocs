//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SPECIFIERS_OPERATORKIND_HPP
#define MRDOCS_API_METADATA_SPECIFIERS_OPERATORKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/Dom/String.hpp>
#include <string>

namespace clang::mrdocs {

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

/** Return the name of an operator as a string.

    @param kind The kind of operator.
    @param include_keyword Whether the name
    should be prefixed with the `operator` keyword.
*/
MRDOCS_DECL
std::string_view
getOperatorName(
    OperatorKind kind,
    bool include_keyword = false) noexcept;

/** Return the short name of an operator as a string.
*/
MRDOCS_DECL
std::string_view
getShortOperatorName(
    OperatorKind kind) noexcept;

/** Return the short name of an operator as a string.

    @param name The operator name, e.g. `operator+`, `operator++`, `operator[]`, etc.
    @return The OperatorKind, or OperatorKind::None if not recognized.
*/
MRDOCS_DECL
OperatorKind
getOperatorKind(std::string_view name) noexcept;

/** Return the short name of an operator as a string.

    @param suffix The operator suffix, e.g. `+`, `++`, `[]`, etc.
    @return The OperatorKind, or OperatorKind::None if not recognized.
*/
MRDOCS_DECL
OperatorKind
getOperatorKindFromSuffix(std::string_view suffix) noexcept;

/** Return the safe name of an operator as a string.

    @param kind The kind of operator.
    @param include_keyword Whether the name
    should be prefixed with `operator_`.
*/
MRDOCS_DECL
std::string_view
getSafeOperatorName(
    OperatorKind kind,
    bool include_keyword = false) noexcept;

/** Return the human-readable name of the operator

    @param kind The kind of operator.
    @param nParams The number of parameters the operator takes.
    @return The readable name, or nullopt if the operator is not recognized.
 */
Optional<std::string_view>
getOperatorReadableName(
    OperatorKind kind,
    int nParams);

} // clang::mrdocs

#endif
