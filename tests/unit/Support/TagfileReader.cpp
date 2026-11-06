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
#include <mrdocs/AST/ParseRef.hpp>
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

    /*  A file with no root element is what a failed download leaves, so
        it is rejected, while an empty root documents nothing.
    */
    void
    testEmptyFile()
    {
        TagfileIndex index;
        BOOST_TEST(!readTagfile(index, "", baseUrl_));
        BOOST_TEST(!readTagfile(index,
            "<?xml version=\"1.0\"?>\n", baseUrl_));
        BOOST_TEST(!readTagfile(index, "</tagfile>", baseUrl_));
        BOOST_TEST(readTagfile(index, "<tagfile/>", baseUrl_).has_value());
        BOOST_TEST(readTagfile(index,
            "<tagfile></tagfile>", baseUrl_).has_value());
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
<compound kind="namespace">
  <name>ns</name>
  <member kind="function">
    <name>operator&lt;&lt;</name>
    <anchorfile>ns/shift.html</anchorfile>
  </member>
  <member kind="function">
    <name>operator&amp;&amp;</name>
    <anchorfile>ns/and.html</anchorfile>
  </member>
</compound>
</tagfile>
)");
        BOOST_TEST(index.find("ns::operator<<") ==
            "https://example.org/ref/ns/shift.html");
        BOOST_TEST(index.find("ns::operator&&") ==
            "https://example.org/ref/ns/and.html");
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

    /*  Doxygen lists the members of the global namespace in the compound
        of the file declaring them, which is not a symbol itself.
    */
    void
    testFileCompound()
    {
        TagfileIndex const index = read(
            R"(<tagfile>
<compound kind="file">
  <name>core.h</name>
  <filename>core_8h.html</filename>
  <namespace>ns</namespace>
  <member kind="function">
    <name>f</name>
    <anchorfile>core_8h.html</anchorfile>
    <anchor>a1</anchor>
  </member>
  <member kind="variable">
    <name>v</name>
    <anchor>a2</anchor>
  </member>
</compound>
</tagfile>
)");
        BOOST_TEST(index.find("f") ==
            "https://example.org/ref/core_8h.html#a1");
        BOOST_TEST(index.find("v") ==
            "https://example.org/ref/core_8h.html#a2");
        BOOST_TEST(!index.find("core.h"));
        BOOST_TEST(!index.find("core.h::f"));
        BOOST_TEST(index.size() == 2);
    }

    // The key a reference written as `name` looks an entry up by.
    static
    std::string
    referenceKey(std::string_view name)
    {
        Expected<ParsedRef> const ref = parse<ParsedRef>(name);
        BOOST_TEST(ref.has_value());
        return ref.has_value() ? tagfileKey(*ref) : std::string();
    }

    /*  Some tagfiles, cppreference's among them, write a member's
        parameter list into its name, or the header that tells an
        overload apart. An entry is keyed as a reference would be,
        without either, except for the `()` that is part of the name of
        a call operator.
    */
    void
    testParameterListInName()
    {
        TagfileIndex const index = read(
            R"(<tagfile>
<compound kind="class">
  <name>ns::vec</name>
  <filename>ns/vec.html</filename>
  <member kind="function">
    <name>operator[](size_type)</name>
    <anchorfile>ns/vec/subscript.html</anchorfile>
  </member>
  <member kind="function">
    <name>at (size_type) const</name>
    <anchorfile>ns/vec/at.html</anchorfile>
  </member>
  <member kind="function">
    <name>operator()(int)</name>
    <anchorfile>ns/vec/call.html</anchorfile>
  </member>
  <member kind="function">
    <name>operator()</name>
    <anchorfile>ns/vec/call-again.html</anchorfile>
  </member>
  <member kind="function">
    <name>operator bool</name>
    <anchorfile>ns/vec/conversion.html</anchorfile>
  </member>
  <member kind="function">
    <name>operator vec&lt;&gt;</name>
    <anchorfile>ns/vec/to-default.html</anchorfile>
  </member>
  <member kind="function">
    <name>find (&lt;algorithm&gt;)</name>
    <anchorfile>ns/vec/find.html</anchorfile>
  </member>
  <member kind="function">
    <name>find (&lt;string&gt;)</name>
    <anchorfile>ns/vec/find-string.html</anchorfile>
  </member>
</compound>
</tagfile>
)");
        BOOST_TEST(index.find(referenceKey(
            "ns::vec::operator bool")) ==
            "https://example.org/ref/ns/vec/conversion.html");
        BOOST_TEST(index.find(referenceKey(
            "ns::vec::operator[](size_type)")) ==
            "https://example.org/ref/ns/vec/subscript.html");
        BOOST_TEST(index.find("ns::vec::operator[]") ==
            "https://example.org/ref/ns/vec/subscript.html");
        BOOST_TEST(index.find("ns::vec::at") ==
            "https://example.org/ref/ns/vec/at.html");
        BOOST_TEST(index.find("ns::vec::operator()") ==
            "https://example.org/ref/ns/vec/call.html");
        BOOST_TEST(index.find("ns::vec::find") ==
            "https://example.org/ref/ns/vec/find.html");
        BOOST_TEST(index.size() == 7);
    }

    /*  A compound whose name is not a symbol, and which does not list the
        global namespace either, contributes nothing.
    */
    void
    testNonScopeCompounds()
    {
        TagfileIndex const index = read(
            R"(<tagfile>
<compound kind="page">
  <name>intro</name>
  <filename>intro.html</filename>
</compound>
<compound kind="group">
  <name>algorithms</name>
  <filename>group__algorithms.html</filename>
  <member kind="function">
    <name>sort</name>
    <anchorfile>group__algorithms.html</anchorfile>
    <anchor>a1</anchor>
  </member>
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
        // A character reference with something after its digits.
        BOOST_TEST(!readTagfile(index,
            "<tagfile><compound kind=\"class\"><name>&#65x;</name>"
            "</compound></tagfile>", baseUrl_));
        // A character reference to a control character XML forbids.
        BOOST_TEST(!readTagfile(index,
            "<tagfile><compound kind=\"class\"><name>&#0;</name>"
            "</compound></tagfile>", baseUrl_));
        // A closing tag for an element that is not the one open: in a
        // member, a compound the reader records, and one it skips.
        BOOST_TEST(!readTagfile(index,
            "<tagfile><compound kind=\"class\"><member><name>a</name>"
            "</compound></member></compound></tagfile>", baseUrl_));
        BOOST_TEST(!readTagfile(index,
            "<tagfile><compound kind=\"class\"><name>a</filename>"
            "</compound></tagfile>", baseUrl_));
        BOOST_TEST(!readTagfile(index,
            "<tagfile><compound kind=\"file\"><x></compound></x>"
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

    /*  A specialization cannot be told from its primary template by a
        reference, so its compound is skipped, members and all, wherever
        the tagfile lists it.
    */
    void
    testSpecializationIsSkipped()
    {
        TagfileIndex const index = read(
            R"(<tagfile>
<compound kind="struct">
  <name>ns::box&lt; 3 &gt;</name>
  <filename>ns/box-3.html</filename>
  <member kind="function">
    <name>volume</name>
    <anchorfile>ns/box-3/volume.html</anchorfile>
  </member>
</compound>
<compound kind="struct">
  <name>ns::box</name>
  <filename>ns/box.html</filename>
  <member kind="function">
    <name>volume</name>
    <anchorfile>ns/box/volume.html</anchorfile>
  </member>
</compound>
</tagfile>
)");
        BOOST_TEST(index.find("ns::box") ==
            "https://example.org/ref/ns/box.html");
        BOOST_TEST(index.find("ns::box::volume") ==
            "https://example.org/ref/ns/box/volume.html");
        BOOST_TEST(index.size() == 2);
    }

    /*  A page is relative to the documentation set, so one written with
        a URL scheme is not a page, and under an empty location it would
        be the whole link.
    */
    void
    testPageWithASchemeIsSkipped()
    {
        TagfileIndex index;
        BOOST_TEST(readTagfile(index,
            "<tagfile><compound kind=\"class\"><name>a</name>"
            "<filename>javascript:alert(1)</filename></compound>"
            "<compound kind=\"class\"><name>b</name>"
            "<filename>b.html?x=1:2</filename></compound></tagfile>",
            "").has_value());
        BOOST_TEST(!index.find("a"));
        BOOST_TEST(index.find("b") == "b.html?x=1:2");
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
        testFileCompound();
        testParameterListInName();
        testNonScopeCompounds();
        testRejectsWhatItCannotRead();
        testTruncatedFile();
        testDeclarationForms();
        testFirstTargetWins();
        testSpecializationIsSkipped();
        testPageWithASchemeIsSkipped();
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
