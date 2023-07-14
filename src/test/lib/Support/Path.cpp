//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "lib/Support/Path.hpp"
#include <test_suite/test_suite.hpp>

namespace clang {
namespace mrdox {

struct Path_test
{
    void
    testPaths()
    {
        using namespace files;
        
    /*
        BOOST_TEST(isAbsolute("C:\\"));
        BOOST_TEST(isAbsolute("/"));
        BOOST_TEST(isAbsolute("/etc"));
        BOOST_TEST(isAbsolute("\\"));
    */
    }

    void run()
    {
        testPaths();
    }
};

TEST_SUITE(
    Path_test,
    "clang.mrdox.Path");

} // mrdox
} // clang

