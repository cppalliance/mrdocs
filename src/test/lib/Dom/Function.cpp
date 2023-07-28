//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Dom/Function.hpp>
#include <test_suite/test_suite.hpp>

namespace clang {
namespace mrdox {
namespace dom {

struct Function_test
{
    void
    testFunction()
    {
        makeInvocable([](int i) {
            BOOST_TEST(i == 0);
            return 0; })(0);
        makeInvocable([](int i) {
            BOOST_TEST(i == 999);
            return 0; })(999);
        makeInvocable([](std::string_view s) {
            BOOST_TEST(s == "test");
            return 0; })("test");
    }

    void run()
    {
        testFunction();
    }
};

TEST_SUITE(
    Function_test,
    "clang.mrdox.dom.Function");

} // dom
} // mrdox
} // clang

