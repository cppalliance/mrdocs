//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Dom.hpp>
#include <test_suite/test_suite.hpp>

namespace clang {
namespace mrdox {
namespace dom {

struct Dom_test
{
    void run()
    {
    }
};

TEST_SUITE(
    Dom_test,
    "clang.mrdox.dom");

} // dom
} // mrdox
} // clang

