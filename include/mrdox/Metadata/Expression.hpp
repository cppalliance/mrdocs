//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_METADATA_EXPRESSION_HPP
#define MRDOX_API_METADATA_EXPRESSION_HPP

#include <mrdox/Platform.hpp>
#include <concepts>
#include <optional>
#include <string>

namespace clang {
namespace mrdox {

/** Represents an expression */
struct ExprInfo
{
    /** The expression, as written */
    std::string Written;
};

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
    std::optional<type> Value;

    static_assert(std::integral<type>,
        "expression type must be integral");
};

} // clang
} // mrdox

#endif
