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

using detail::TagfileIndex;

struct TagfileReaderTest
{
    static
    constexpr std::string_view baseUrl_ = "https://example.org/ref/";

    // Read a tagfile that is expected to be readable.
    static
    TagfileIndex
    read(std::string_view contents)
    {
        TagfileIndex index;
        Expected<void> const result = readTagfile(index, contents, baseUrl_);
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

    // A tagfile can name a symbol without saying where it is documented,
    // and such an entry would link to the top of the documentation set
    // rather than to the symbol.
    void
    testEntriesWithoutAPage()
    {
        TagfileIndex const index = read(
            R"(<tagfile>
<compound kind="class">
  <name>ns::c</name>
  <member kind="function">
    <name>f</name>
    <anchor>a1</anchor>
  </member>
</compound>
</tagfile>
)");
        BOOST_TEST(index.empty());
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

    // A base URL is configured by hand, so it may or may not have the
    // trailing slash the join needs.
    void
    testBaseUrlSlash()
    {
        std::string_view const contents =
            "<tagfile><compound kind=\"class\"><name>a</name>"
            "<filename>p.html</filename></compound></tagfile>";
        for (std::string_view const base: {
            "https://example.org/d/",
            "https://example.org/d"})
        {
            TagfileIndex index;
            BOOST_TEST(readTagfile(index, contents, base).has_value());
            BOOST_TEST(index.find("a") == "https://example.org/d/p.html");
        }
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
        // The same, inside a compound the reader skips.
        BOOST_TEST(!readTagfile(index,
            "<tagfile><compound kind=\"file\"><name>&nbsp;</name>"
            "</compound></tagfile>", baseUrl_));
        // A character reference to a surrogate, which is no character.
        BOOST_TEST(!readTagfile(index,
            "<tagfile><compound kind=\"class\"><name>&#xD800;</name>"
            "</compound></tagfile>", baseUrl_));
        // A character reference past the end of Unicode.
        BOOST_TEST(!readTagfile(index,
            "<tagfile><compound kind=\"class\"><name>&#x110000;</name>"
            "</compound></tagfile>", baseUrl_));
        // Text where the root element should be, as in a JSON file.
        BOOST_TEST(!readTagfile(index, "{\"a\": 1}", baseUrl_));
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

    /*  The declaration is skipped whole, so every way of writing it reads
        the same: ours, Doxygen's (single quotes, a space before the
        `?>`), a lowercase encoding, no encoding, a byte order mark ahead
        of it, or no declaration at all.
    */
    void
    testDeclarationForms()
    {
        for (std::string_view const declaration: {
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>",
            "<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>",
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>",
            "<?xml version=\"1.0\"?>",
            "\xEF\xBB\xBF<?xml version=\"1.0\" encoding=\"UTF-8\"?>",
            ""})
        {
            std::string contents(declaration);
            contents +=
                "\n<tagfile><compound kind=\"class\"><name>ns::c</name>"
                "<filename>c.html</filename></compound></tagfile>\n";
            TagfileIndex const index = read(contents);
            BOOST_TEST(index.find("ns::c") ==
                "https://example.org/ref/c.html");
        }
    }

    /*  The error names the line the reader is on when it meets the text.
        A newline written as a character reference is not a line break.
    */
    void
    testTextNamesItsLine()
    {
        TagfileIndex index;
        Expected<void> const json =
            readTagfile(index, "\n{\"a\": 1}", baseUrl_);
        BOOST_TEST(!json.has_value());
        BOOST_TEST(json.error().reason() ==
            "text where a tagfile begins, on line 2");
        Expected<void> const reference =
            readTagfile(index, "x&#10;<tagfile/>", baseUrl_);
        BOOST_TEST(!reference.has_value());
        BOOST_TEST(reference.error().reason() ==
            "text where a tagfile begins, on line 1");
        Expected<void> const references =
            readTagfile(index, "x&#10;&#10;<tagfile/>", baseUrl_);
        BOOST_TEST(!references.has_value());
        BOOST_TEST(references.error().reason() ==
            "text where a tagfile begins, on line 1");
    }

    /*  MrDocs writes the global namespace as a compound with an empty
        name, whose members are then keyed by their bare names.
    */
    void
    testGlobalNamespaceMembers()
    {
        TagfileIndex const index = read(
            R"(<tagfile>
<compound kind="namespace">
  <name/>
  <filename>index.html</filename>
  <member kind="function">
    <type>void</type>
    <name>init</name>
    <anchorfile>init.html</anchorfile>
    <anchor/>
    <arglist>()</arglist>
  </member>
</compound>
</tagfile>
)");
        BOOST_TEST(index.find("init") ==
            "https://example.org/ref/init.html");
        BOOST_TEST(!index.find("::init"));
        BOOST_TEST(index.size() == 1);
    }

    // Doxygen writes a concept as a compound of its own.
    void
    testConceptCompound()
    {
        TagfileIndex const index = read(
            R"(<tagfile>
<compound kind="concept">
  <name>ns::shape_like</name>
  <filename>conceptns_1_1shape__like.html</filename>
</compound>
</tagfile>
)");
        BOOST_TEST(index.find("ns::shape_like") ==
            "https://example.org/ref/conceptns_1_1shape__like.html");
    }

    void
    testMissingFile()
    {
        TagfileIndex index;
        std::string_view const path = "no/such/dir/missing.tag.xml";
        Expected<void> const result = loadTagfile(index, path, baseUrl_);
        BOOST_TEST(!result.has_value());
        BOOST_TEST(result.error().reason().find(path) !=
            std::string::npos);
    }

    void
    run()
    {
        testEmptyFile();
        testCompoundAndMember();
        testAnchorOnThePage();
        testMemberWithoutAnchorFile();
        testEntriesWithoutAPage();
        testEntities();
        testCharacterReferences();
        testBaseUrlSlash();
        testUnknownElementsAreSkipped();
        testNonScopeCompounds();
        testRejectsWhatItCannotRead();
        testTruncatedFile();
        testDeclarationForms();
        testFirstTargetWins();
        testTextNamesItsLine();
        testGlobalNamespaceMembers();
        testConceptCompound();
        testMissingFile();
    }
};

TEST_SUITE(
    TagfileReaderTest,
    "clang.mrdocs.TagfileReader");

} // mrdocs
