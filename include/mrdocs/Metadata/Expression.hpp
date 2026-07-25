//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2025 Gennaro Prota (gennaro.prota@gmail.com)
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_EXPRESSION_HPP
#define MRDOCS_API_METADATA_EXPRESSION_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/Dom/LazyObject.hpp>
#include <mrdocs/Dom/Value.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <concepts>
#include <optional>
#include <string>

namespace mrdocs {

class DomCorpus;

/** Represents an expression
*/
struct ExprInfo
{
    /** The expression, as written
    */
    std::string Written;

    /** View this object as its base expression.
    */
    ExprInfo& asExpr() noexcept
    {
        return *this;
    }

    /** View this object as its base expression.
    */
    ExprInfo const& asExpr() const noexcept
    {
        return *this;
    }

    /** Order expressions by written form.
    */
    auto operator<=>(ExprInfo const&) const = default;
};

MRDOCS_DESCRIBE_STRUCT(ExprInfo, (), (Written))

/** Merge metadata from another expression.
*/
MRDOCS_DECL
void
merge(ExprInfo& I, ExprInfo&& Other);

/** Represents an expression with a (possibly known) value
*/
template<typename T>
struct ConstantExprInfo
    : ExprInfo
{
    /** The underlying type of the expression
    */
    using type = T;

    /** The expressions value, if it is known

        The value of an expression will be unknown
        if it is e.g. dependent on a template parameter
    */
    Optional<type> Value;

    /** Order constant expressions by written form and value.
    */
    auto operator<=>(ConstantExprInfo const&) const = default;

    MRDOCS_DESCRIBE_CLASS(ConstantExprInfo, (ExprInfo), (Value))

    static_assert(std::integral<type>, "expression type must be integral");
};

/** Merge metadata from another constant expression.
*/
template <class T>
static void merge(
    ConstantExprInfo<T>& I,
    ConstantExprInfo<T>&& Other)
{
    merge(I.asExpr(), std::move(Other.asExpr()));
    if (!I.Value)
    {
        I.Value = std::move(Other.Value);
    }
}

/** Map an ExprInfo to a @ref dom::Value object.

    @param v The output parameter to receive the dom::Value.
    @param expr The expression info to convert.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    ExprInfo const& expr,
    DomCorpus const*)
{
    v = expr.Written;
}

/** Map an ExprInfo to a @ref dom::Value object.

    @param v The output parameter to receive the dom::Value.
    @param expr The expression info to convert.
*/
inline
void
tag_invoke(
    dom::LazyObjectMapTag,
    dom::Value& v,
    ExprInfo const& expr,
    DomCorpus const*
)
{
    v = expr.Written;
}

} // mrdocs

#endif
