//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_EXPRESSION_HPP
#define MRDOCS_API_METADATA_EXPRESSION_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Optional.hpp>
#include <concepts>
#include <optional>
#include <string>

namespace clang::mrdocs {

/** Represents an expression */
struct ExprInfo
{
    /** The expression, as written */
    std::string Written;

    ExprInfo& asExpr() noexcept
    {
        return *this;
    }

    ExprInfo const& asExpr() const noexcept
    {
        return *this;
    }

    auto operator<=>(ExprInfo const&) const = default;
};

MRDOCS_DECL
void
merge(ExprInfo& I, ExprInfo&& Other);

/** Represents an expression with a (possibly known) value */
template<typename T>
struct ConstantExprInfo
    : ExprInfo
{
    /** The underlying type of the expression */
    using type = T;

    /** The expressions value, if it is known

        The value of an expression will be unknown
        if it is e.g. dependent on a template parameter
    */
    Optional<type> Value;

    auto operator<=>(ConstantExprInfo const&) const = default;

    static_assert(std::integral<type>, "expression type must be integral");
};

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

} // clang::mrdocs

#endif
