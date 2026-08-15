//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef EXAMPLE_CALCULATOR_HPP
#define EXAMPLE_CALCULATOR_HPP

namespace example {

/** A tiny calculator.

    This type exists only to give the mrdocs CMake extension a documented
    symbol to extract from this consumer project.
*/
class Calculator
{
public:
    /// Return the sum of two integers.
    int add(int a, int b) const;

    /// Return the product of two integers.
    int multiply(int a, int b) const;
};

} // namespace example

#endif
