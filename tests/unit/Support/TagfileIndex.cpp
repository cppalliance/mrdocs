//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/detail/TagfileIndex.hpp>
#include <test_suite/test_suite.hpp>
#include <string>

namespace mrdocs {

using detail::TagfileIndex;

struct TagfileIndexTest
{
    void
    testEmpty()
    {
        TagfileIndex index;
        BOOST_TEST(index.empty());
        BOOST_TEST(index.size() == 0);
        BOOST_TEST(!index.find("boost::urls::url"));
        BOOST_TEST(!index.contains("boost::urls::url"));
    }

    void
    testFind()
    {
        TagfileIndex index;
        BOOST_TEST(index.insert(
            "boost::urls::url",
            "https://example.org/url/classes/url.html"));
        BOOST_TEST(!index.empty());
        BOOST_TEST(index.size() == 1);
        BOOST_TEST(index.find("boost::urls::url") ==
            "https://example.org/url/classes/url.html");
    }

    // Only the whole qualified name matches.
    void
    testExactNames()
    {
        TagfileIndex index;
        BOOST_TEST(index.insert(
            "boost::urls::url", "https://example.org/p.html"));
        BOOST_TEST(!index.find("url"));
        BOOST_TEST(!index.find("urls::url"));
        BOOST_TEST(!index.find("boost::urls"));
        BOOST_TEST(!index.find("boost::urls::url::clear"));
        BOOST_TEST(index.find("boost::urls::url"));
        BOOST_TEST(index.contains("boost::urls::url"));
        BOOST_TEST(!index.contains("urls::url"));
    }

    void
    testFirstOneWins()
    {
        TagfileIndex index;
        BOOST_TEST(index.insert("a", "https://first.example/p.html"));
        BOOST_TEST(!index.insert("a", "https://second.example/q.html"));
        BOOST_TEST(index.find("a") == "https://first.example/p.html");
    }

    // An entry needs both a name and a URL.
    void
    testUnusableEntries()
    {
        TagfileIndex index;
        BOOST_TEST(!index.insert("", "https://example.org/p.html"));
        BOOST_TEST(!index.insert("a", ""));
        BOOST_TEST(index.empty());
    }

    void
    run()
    {
        testEmpty();
        testFind();
        testExactNames();
        testFirstOneWins();
        testUnusableEntries();
    }
};

TEST_SUITE(
    TagfileIndexTest,
    "clang.mrdocs.TagfileIndex");

} // mrdocs
