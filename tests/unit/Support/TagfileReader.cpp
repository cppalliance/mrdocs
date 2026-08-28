//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Support/TagfileReader.hpp>
#include <test_suite/test_suite.hpp>
#include <string>
#include <string_view>

namespace mrdocs {

struct TagfileReaderTest
{
    static
    constexpr std::string_view baseUrl_ = "https://example.org/ref/";

    // Read a tagfile that is expected to be readable.
    static
    TagfileIndex
    read(std::string_view const contents)
    {
        TagfileIndex index;
        auto const result = readTagfile(index, contents, baseUrl_);
        BOOST_TEST(result.has_value());
        return index;
    }

    void
    testEmptyFile()
    {
        TagfileIndex index;
        BOOST_TEST(readTagfile(index, "", baseUrl_).has_value());
        BOOST_TEST(index.empty());
    }

    void
    testCompoundAndMember()
    {
        TagfileIndex const index = read(
            R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<tagfile>
<compound kind="class">
  <name>ns::outer</name>
  <filename>ns/outer.html</filename>
  <member kind="function">
    <type>void</type>
    <name>method</name>
    <anchorfile>ns/outer/method.html</anchorfile>
    <anchor/>
    <arglist>()</arglist>
  </member>
</compound>
</tagfile>
)");
        BOOST_TEST(index.find("ns::outer") ==
            "https://example.org/ref/ns/outer.html");
        BOOST_TEST(index.find("ns::outer::method") ==
            "https://example.org/ref/ns/outer/method.html");
    }

    void
    testAnchorOnThePage()
    {
        TagfileIndex const index = read(
            R"(<tagfile>
<compound kind="namespace">
  <name>ns</name>
  <filename>ns.html</filename>
  <member kind="function">
    <name>f</name>
    <anchorfile>ns.html</anchorfile>
    <anchor>a1b2c3</anchor>
  </member>
</compound>
</tagfile>
)");
        BOOST_TEST(index.find("ns::f") ==
            "https://example.org/ref/ns.html#a1b2c3");
    }

    // A member with no anchorfile is documented on the compound's page.
    void
    testMemberWithoutAnchorFile()
    {
        TagfileIndex const index = read(
            R"(<tagfile>
<compound kind="struct">
  <name>ns::s</name>
  <filename>ns/s.html</filename>
  <member kind="variable">
    <name>value</name>
    <anchor>abc</anchor>
  </member>
</compound>
</tagfile>
)");
        BOOST_TEST(index.find("ns::s::value") ==
            "https://example.org/ref/ns/s.html#abc");
    }

    void
    testEntities()
    {
        TagfileIndex const index = read(
            R"(<tagfile>
<compound kind="class">
  <name>ns::vec&lt;T&amp;&gt;</name>
  <filename>ns/vec.html</filename>
</compound>
</tagfile>
)");
        BOOST_TEST(index.find("ns::vec<T&>") ==
            "https://example.org/ref/ns/vec.html");
    }

    void
    testCharacterReferences()
    {
        TagfileIndex const index = read(
            R"(<tagfile>
<compound kind="namespace">
  <name>ns&#58;&#x3A;inner</name>
  <filename>ns/inner.html</filename>
</compound>
</tagfile>
)");
        BOOST_TEST(index.find("ns::inner") ==
            "https://example.org/ref/ns/inner.html");
    }

    // The elements Doxygen writes and a tagfile reader has no use for,
    // including ones with children of their own.
    void
    testUnknownElementsAreSkipped()
    {
        TagfileIndex const index = read(
            R"(<?xml version="1.0"?>
<!-- a comment, and one <compound> that is not real -->
<tagfile doxygen_version="1.9.8">
<compound kind="class">
  <name>ns::derived</name>
  <filename>ns/derived.html</filename>
  <base protection="public">ns::base</base>
  <templarg><type>class T</type></templarg>
  <class kind="class">ns::derived::nested</class>
  <member kind="enumeration">
    <name>color</name>
    <anchorfile>ns/derived.html</anchorfile>
    <anchor>e1</anchor>
    <enumvalue file="ns/derived.html" anchor="v1">red</enumvalue>
  </member>
  <docanchor file="ns/derived.html">notes</docanchor>
</compound>
</tagfile>
)");
        BOOST_TEST(index.find("ns::derived") ==
            "https://example.org/ref/ns/derived.html");
        BOOST_TEST(index.find("ns::derived::color") ==
            "https://example.org/ref/ns/derived.html#e1");
        // The nested class is named by a `<class>` reference here, and is
        // recorded from its own compound instead, so nothing about it is
        // learned from this one.
        BOOST_TEST(!index.find("ns::derived::nested"));
        BOOST_TEST(!index.find("ns::base"));
    }

    // A compound whose name is not a symbol contributes nothing, and in
    // particular does not put its members in the index unqualified.
    void
    testNonScopeCompounds()
    {
        TagfileIndex const index = read(
            R"(<tagfile>
<compound kind="file">
  <name>core.hpp</name>
  <filename>core_8hpp.html</filename>
  <member kind="function">
    <name>f</name>
    <anchorfile>core_8hpp.html</anchorfile>
    <anchor>a1</anchor>
  </member>
</compound>
<compound kind="page">
  <name>intro</name>
  <filename>intro.html</filename>
</compound>
<compound kind="group">
  <name>algorithms</name>
  <filename>group__algorithms.html</filename>
</compound>
</tagfile>
)");
        BOOST_TEST(index.empty());
    }

    void
    testRejectsWhatItCannotRead()
    {
        TagfileIndex index;
        // A document type declaration.
        BOOST_TEST(!readTagfile(index,
            "<!DOCTYPE tagfile><tagfile></tagfile>", baseUrl_));
        // A character data section.
        BOOST_TEST(!readTagfile(index,
            "<tagfile><compound kind=\"class\"><name><![CDATA[x]]></name>"
            "</compound></tagfile>", baseUrl_));
        // Something that is not a tagfile at all.
        BOOST_TEST(!readTagfile(index,
            "<doxygenindex></doxygenindex>", baseUrl_));
        // A tag that never ends.
        BOOST_TEST(!readTagfile(index, "<tagfile><compound kind=", baseUrl_));
        // An entity reference nothing defines.
        BOOST_TEST(!readTagfile(index,
            "<tagfile><compound kind=\"class\"><name>&nbsp;</name>"
            "</compound></tagfile>", baseUrl_));
        // Nothing was recorded by any of them.
        BOOST_TEST(index.empty());
    }

    /*  A file that stops in the middle is rejected rather than read as
        far as it goes.
    */
    void
    testTruncatedFile()
    {
        TagfileIndex index;
        // The root element is left open.
        BOOST_TEST(!readTagfile(index,
            "<tagfile><compound kind=\"class\"><name>ns::c</name>"
            "<filename>c.html</filename></compound>", baseUrl_));
        // A compound is left open.
        BOOST_TEST(!readTagfile(index,
            "<tagfile><compound kind=\"class\"><name>ns::c</name>",
            baseUrl_));
    }

    void
    testFirstTargetWins()
    {
        TagfileIndex const index = read(
            R"(<tagfile>
<compound kind="class">
  <name>ns::c</name>
  <filename>first.html</filename>
</compound>
<compound kind="class">
  <name>ns::c</name>
  <filename>second.html</filename>
</compound>
</tagfile>
)");
        BOOST_TEST(index.find("ns::c") ==
            "https://example.org/ref/first.html");
    }

    void
    run()
    {
        testEmptyFile();
        testCompoundAndMember();
        testAnchorOnThePage();
        testMemberWithoutAnchorFile();
        testEntities();
        testCharacterReferences();
        testUnknownElementsAreSkipped();
        testNonScopeCompounds();
        testRejectsWhatItCannotRead();
        testTruncatedFile();
        testFirstTargetWins();
    }
};

TEST_SUITE(
    TagfileReaderTest,
    "clang.mrdocs.TagfileReader");

} // mrdocs
