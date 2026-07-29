//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <test_suite/test_suite.hpp>

// Entry point for the standalone dom test suite. This executable links only the
// dom library, the polyfills it depends on, and the test framework: no
// mrdocs-core and no LLVM. A dom test failure therefore points at dom alone.
int
main(int argc, char const** argv)
{
    return test_suite::unit_test_main(argc, argv);
}
