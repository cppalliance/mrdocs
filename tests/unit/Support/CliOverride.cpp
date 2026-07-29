//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Support/CliOverride.hpp>
#include <mrdocs/Dom/Object.hpp>
#include <mrdocs/Dom/Value.hpp>
#include <test_suite/test_suite.hpp>
#include <array>
#include <map>
#include <string>
#include <vector>

namespace mrdocs {

namespace {

// Build a null-terminated argv from a list of arguments, the shape the
// command-line parser hands to applyDottedObjectOverrides.
std::vector<char const*>
makeArgv(std::vector<char const*> args)
{
    args.push_back(nullptr);
    return args;
}

} // (anon)

struct CliOverrideTest
{
    void
    testParseScalar()
    {
        // true/false resolve to a Boolean (so multipage reads back as one).
        BOOST_TEST(parseCliScalarValue("true").isBoolean());
        BOOST_TEST(parseCliScalarValue("true").getBool() == true);
        BOOST_TEST(parseCliScalarValue("false").isBoolean());
        BOOST_TEST(parseCliScalarValue("false").getBool() == false);

        // Base-10 integers resolve to an Integer.
        BOOST_TEST(parseCliScalarValue("42").isInteger());
        BOOST_TEST(parseCliScalarValue("42").getInteger() == 42);
        BOOST_TEST(parseCliScalarValue("-7").isInteger());
        BOOST_TEST(parseCliScalarValue("-7").getInteger() == -7);

        // Anything else stays a String, including near-misses.
        BOOST_TEST(parseCliScalarValue("docs/html").isString());
        BOOST_TEST(std::string_view(
            parseCliScalarValue("docs/html").getString()) == "docs/html");
        BOOST_TEST(parseCliScalarValue("1.5").isString());
        BOOST_TEST(parseCliScalarValue("truthy").isString());
        BOOST_TEST(parseCliScalarValue("").isString());
    }

    void
    testIsDottedObjectOverride()
    {
        std::array<std::string_view, 1> const names{"generator-options"};

        // A dotted key whose head matches a named object option.
        BOOST_TEST(isDottedObjectOverride(
            "--generator-options.html.output=x", names));
        BOOST_TEST(isDottedObjectOverride(
            "--generator-options.html=x", names));

        // No dot, so not a nested override (these are registered flags).
        BOOST_TEST(!isDottedObjectOverride("--generator=xml,html", names));
        BOOST_TEST(!isDottedObjectOverride("--multipage=true", names));

        // Head is not one of the object options.
        BOOST_TEST(!isDottedObjectOverride("--parent.child=v", names));

        // Not an option token at all.
        BOOST_TEST(!isDottedObjectOverride(
            "generator-options.html.output=x", names));
    }

    void
    testApplyOverrides()
    {
        std::map<std::string, dom::Object> target;
        auto argv = makeArgv({
            "mrdocs",
            "--generator=xml,html,adoc",
            "--generator-options.html.output=docs/html",
            "--generator-options.html.multipage=true",
            "--generator-options.xml.output=docs/xml",
            "--generator-options.xml.multipage=false",
            "--unrelated=foo"});

        auto const result = applyDottedObjectOverrides(
            target, "generator-options", argv.data());
        BOOST_TEST(result.has_value());

        // Only the two generator ids became map entries.
        BOOST_TEST(target.size() == 2u);
        BOOST_TEST(target.contains("html"));
        BOOST_TEST(target.contains("xml"));

        // Values land under the right key with the right type.
        BOOST_TEST(std::string_view(
            target["html"].get("output").getString()) == "docs/html");
        BOOST_TEST(target["html"].get("multipage").isBoolean());
        BOOST_TEST(target["html"].get("multipage").getBool() == true);
        BOOST_TEST(std::string_view(
            target["xml"].get("output").getString()) == "docs/xml");
        BOOST_TEST(target["xml"].get("multipage").isBoolean());
        BOOST_TEST(target["xml"].get("multipage").getBool() == false);
    }

    void
    testMergeOntoExisting()
    {
        // A value already loaded from the config file.
        dom::Object html;
        html.set("output", "from-config");
        html.set("keep-me", "untouched");
        std::map<std::string, dom::Object> target;
        target.emplace("html", html);

        auto argv = makeArgv({
            "mrdocs", "--generator-options.html.output=from-cli"});
        auto const result = applyDottedObjectOverrides(
            target, "generator-options", argv.data());
        BOOST_TEST(result.has_value());

        // The override replaces only the addressed field.
        BOOST_TEST(std::string_view(
            target["html"].get("output").getString()) == "from-cli");
        BOOST_TEST(std::string_view(
            target["html"].get("keep-me").getString()) == "untouched");
    }

    void
    testNestedPath()
    {
        std::map<std::string, dom::Object> target;
        auto argv = makeArgv({
            "mrdocs", "--generator-options.html.a.b=c"});
        auto const result = applyDottedObjectOverrides(
            target, "generator-options", argv.data());
        BOOST_TEST(result.has_value());

        dom::Value const a = target["html"].get("a");
        BOOST_TEST(a.isObject());
        BOOST_TEST(std::string_view(a.getObject().get("b").getString()) == "c");
    }

    void
    testMalformed()
    {
        // A key with no field after the map key.
        {
            std::map<std::string, dom::Object> target;
            auto argv = makeArgv({"mrdocs", "--generator-options.html=x"});
            BOOST_TEST(!applyDottedObjectOverrides(
                target, "generator-options", argv.data()).has_value());
        }

        // A dotted override with no "=value".
        {
            std::map<std::string, dom::Object> target;
            auto argv = makeArgv({"mrdocs", "--generator-options.html.output"});
            BOOST_TEST(!applyDottedObjectOverrides(
                target, "generator-options", argv.data()).has_value());
        }
    }

    void
    run()
    {
        testParseScalar();
        testIsDottedObjectOverride();
        testApplyOverrides();
        testMergeOntoExisting();
        testNestedPath();
        testMalformed();
    }
};

TEST_SUITE(
    CliOverrideTest,
    "clang.mrdocs.CliOverride");

} // mrdocs
