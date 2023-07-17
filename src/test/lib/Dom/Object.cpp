//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Dom/Object.hpp>
#include <test_suite/test_suite.hpp>

namespace clang {
namespace mrdox {
namespace dom {

struct Object_test
{
    void
    testObject()
    {
        Object obj({
            { "a", 1 },
            { "b", nullptr },
            { "c", "test" }});
        std::string s;
        for(auto const& kv : obj)
        {
            s += kv.key.str();
        }
    }

    void run()
    {
        testObject();
    }
};

TEST_SUITE(
    Object_test,
    "clang.mrdox.dom.Object");

} // dom
} // mrdox
} // clang

