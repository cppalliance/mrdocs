//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <example/calculator.hpp>

namespace example {

int
Calculator::add(int a, int b) const
{
    return a + b;
}

int
Calculator::multiply(int a, int b) const
{
    return a * b;
}

} // namespace example
