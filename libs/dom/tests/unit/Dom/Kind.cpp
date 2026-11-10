//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Dom.hpp>
#include <test_suite/test_suite.hpp>


namespace mrdocs {
namespace dom {

struct Kind_test
{
    void
    kind_test()
    {
        BOOST_TEST(toString(Kind::Undefined) == "undefined");
        BOOST_TEST(toString(Kind::Null) == "null");
        BOOST_TEST(toString(Kind::Boolean) == "boolean");
        BOOST_TEST(toString(Kind::Integer) == "integer");
        BOOST_TEST(toString(Kind::String) == "string");
        BOOST_TEST(toString(Kind::SafeString) == "safeString");
        BOOST_TEST(toString(Kind::Array) == "array");
        BOOST_TEST(toString(Kind::Object) == "object");
        BOOST_TEST(toString(Kind::Function) == "function");
        BOOST_TEST(toString(static_cast<Kind>(123)) == "unknown");
    }

    void run()
    {
        kind_test();
    }
};

TEST_SUITE(
    Kind_test,
    "clang.mrdocs.dom.Kind");

} // dom
} // mrdocs
