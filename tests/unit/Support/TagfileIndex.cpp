//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Support/TagfileIndex.hpp>
#include <test_suite/test_suite.hpp>
#include <string>

namespace mrdocs {

struct TagfileIndexTest
{
    void
    testEmpty()
    {
        TagfileIndex index;
        BOOST_TEST(index.empty());
        BOOST_TEST(index.size() == 0);
        BOOST_TEST(!index.find("boost::urls::url"));
    }

    void
    testPage()
    {
        TagfileIndex index;
        BOOST_TEST(index.insert(
            "boost::urls::url",
            {"https://example.org/url/", "classes/url.html", ""}));
        BOOST_TEST(!index.empty());
        BOOST_TEST(index.size() == 1);
        BOOST_TEST(index.find("boost::urls::url") ==
            "https://example.org/url/classes/url.html");
    }

    void
    testAnchor()
    {
        TagfileIndex index;
        BOOST_TEST(index.insert(
            "boost::urls::url::clear",
            {"https://example.org/url/", "classes/url.html", "a1b2c3"}));
        BOOST_TEST(index.find("boost::urls::url::clear") ==
            "https://example.org/url/classes/url.html#a1b2c3");
    }

    // A base URL is configured by hand, so it may or may not have the
    // trailing slash the join needs.
    void
    testBaseUrlSlash()
    {
        TagfileIndex withSlash;
        BOOST_TEST(withSlash.insert("a", {"https://example.org/d/", "p.html", ""}));
        BOOST_TEST(withSlash.find("a") == "https://example.org/d/p.html");

        TagfileIndex withoutSlash;
        BOOST_TEST(withoutSlash.insert("a", {"https://example.org/d", "p.html", ""}));
        BOOST_TEST(withoutSlash.find("a") == "https://example.org/d/p.html");
    }

    // Only the whole qualified name matches.
    void
    testExactNames()
    {
        TagfileIndex index;
        BOOST_TEST(index.insert(
            "boost::urls::url", {"https://example.org/", "p.html", ""}));
        BOOST_TEST(!index.find("url"));
        BOOST_TEST(!index.find("urls::url"));
        BOOST_TEST(!index.find("boost::urls"));
        BOOST_TEST(!index.find("boost::urls::url::clear"));
        BOOST_TEST(index.find("boost::urls::url"));
    }

    void
    testFirstOneWins()
    {
        TagfileIndex index;
        BOOST_TEST(index.insert("a", {"https://first.example/", "p.html", ""}));
        BOOST_TEST(!index.insert("a", {"https://second.example/", "q.html", ""}));
        BOOST_TEST(index.find("a") == "https://first.example/p.html");
    }

    // A tagfile can name a symbol without saying where it is documented,
    // and such an entry would link to the top of the documentation set
    // rather than to the symbol.
    void
    testUnusableEntries()
    {
        TagfileIndex index;
        BOOST_TEST(!index.insert("", {"https://example.org/", "p.html", ""}));
        BOOST_TEST(!index.insert("a", {"https://example.org/", "", ""}));
        BOOST_TEST(index.empty());
    }

    void
    run()
    {
        testEmpty();
        testPage();
        testAnchor();
        testBaseUrlSlash();
        testExactNames();
        testFirstOneWins();
        testUnusableEntries();
    }
};

TEST_SUITE(
    TagfileIndexTest,
    "clang.mrdocs.TagfileIndex");

} // mrdocs
