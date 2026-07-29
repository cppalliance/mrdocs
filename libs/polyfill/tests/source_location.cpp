//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/polyfill/source_location.hpp>
#include <test_suite/test_suite.hpp>
#include <string_view>

namespace mrdocs {

struct source_location_test
{
    static
    source_location
    here(source_location loc = source_location::current())
    {
        return loc;
    }

    void
    run()
    {
        source_location const loc = here();
        BOOST_TEST(loc.line() > 0u);
        BOOST_TEST(!std::string_view(loc.file_name()).empty());
        BOOST_TEST(std::string_view(loc.function_name()).size() >= 0u);
    }
};

TEST_SUITE(source_location_test, "mrdocs.polyfills.source_location");

} // namespace mrdocs

int
main(int argc, char const** argv)
{
    return test_suite::unit_test_main(argc, argv);
}
