//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Config.hpp>
#include <test_suite/test_suite.hpp>

namespace mrdocs {

struct ConfigTest
{
    void
    testDeprecatedOptionFromFile()
    {
        // A legacy configuration key forwards its value to the replacement
        // option, so a rename never silently drops a user's setting.
        Config c;
        auto const result = Config::load(
            c, "extract-implicit-specializations: false");
        BOOST_TEST(result.has_value());
        BOOST_TEST(!c.extractImplicitBaseClasses);
    }

    void
    testDeprecatedOptionFromCommandLine()
    {
        char const* argv[] = {
            "mrdocs", "--extract-implicit-specializations=false", nullptr};
        Config c;
        auto const result = Config::load(c, "", argv);
        BOOST_TEST(result.has_value());
        BOOST_TEST(!c.extractImplicitBaseClasses);
    }

    void
    run()
    {
        testDeprecatedOptionFromFile();
        testDeprecatedOptionFromCommandLine();
    }
};

TEST_SUITE(
    ConfigTest,
    "clang.mrdocs.Config");

} // mrdocs
