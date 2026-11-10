//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Generators/hbs/DataDrivenGenerators.hpp>
#include <mrdocs/Generators/hbs/HandlebarsGenerator.hpp>
#include <mrdocs/Support/Filesystem/Temp.hpp>
#include <mrdocs/Handlebars.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <test_suite/test_suite.hpp>
#include <string>
#include <string_view>

namespace mrdocs::hbs {

namespace {

// Apply `map` to `input` and return the escaped result, so tests can
// observe an `EscapeMap`'s contents through its public surface.
std::string
applyEscape(EscapeMap const& map, std::string_view input)
{
    std::string out;
    handlebars::OutputRef ref(out);
    map.apply(ref, input);
    return out;
}

} // (anon)

struct DataDrivenGeneratorsTest
{
    //
    // loadGeneratorMetadata
    //

    void
    testLoadEmptyFile()
    {
        ScopedTempDirectory td("mrdocs-addongen");
        BOOST_TEST(td);
        std::string const path =
            files::appendPath(td.path(), "g.yml");
        BOOST_TEST(files::writeFileText(path, "").has_value());

        Expected<EscapeMap> result = loadGeneratorMetadata(path);
        BOOST_TEST(result.has_value());
        if (result)
        {
            // Empty map: every char passes through.
            BOOST_TEST(applyEscape(*result, "abc*_") == "abc*_");
        }
    }

    void
    testLoadNoEscapeKey()
    {
        ScopedTempDirectory td("mrdocs-addongen");
        BOOST_TEST(td);
        std::string const path =
            files::appendPath(td.path(), "g.yml");
        // Top-level mapping with an unknown key is fine: the schema
        // explicitly tolerates extra keys for forward compatibility.
        BOOST_TEST(files::writeFileText(
            path, "displayName: Markdown\n")
                .has_value());

        Expected<EscapeMap> result = loadGeneratorMetadata(path);
        BOOST_TEST(result.has_value());
        if (result)
        {
            BOOST_TEST(applyEscape(*result, "abc") == "abc");
        }
    }

    void
    testLoadValidEscape()
    {
        ScopedTempDirectory td("mrdocs-addongen");
        BOOST_TEST(td);
        std::string const path =
            files::appendPath(td.path(), "g.yml");
        // Single-quoted YAML scalars treat backslash literally, so the
        // value '\*' is the two-character string  \*.
        BOOST_TEST(files::writeFileText(path,
            "escape:\n"
            "  '*': '\\*'\n"
            "  '_': '\\_'\n")
                .has_value());

        Expected<EscapeMap> result = loadGeneratorMetadata(path);
        BOOST_TEST(result.has_value());
        if (result)
        {
            BOOST_TEST(applyEscape(*result, "*foo_bar*") == "\\*foo\\_bar\\*");
            BOOST_TEST(applyEscape(*result, "no specials") == "no specials");
        }
    }

    void
    testLoadNonMappingTopLevel()
    {
        ScopedTempDirectory td("mrdocs-addongen");
        BOOST_TEST(td);
        std::string const path =
            files::appendPath(td.path(), "g.yml");
        // Top-level scalar is rejected.
        BOOST_TEST(files::writeFileText(path, "just a string\n").has_value());

        Expected<EscapeMap> result = loadGeneratorMetadata(path);
        BOOST_TEST(!result.has_value());
    }

    void
    testLoadNonMappingEscape()
    {
        ScopedTempDirectory td("mrdocs-addongen");
        BOOST_TEST(td);
        std::string const path =
            files::appendPath(td.path(), "g.yml");
        // 'escape:' must be a mapping, not a scalar.
        BOOST_TEST(files::writeFileText(path, "escape: nope\n").has_value());

        Expected<EscapeMap> result = loadGeneratorMetadata(path);
        BOOST_TEST(!result.has_value());
    }

    void
    testLoadMultibyteKey()
    {
        ScopedTempDirectory td("mrdocs-addongen");
        BOOST_TEST(td);
        std::string const path =
            files::appendPath(td.path(), "g.yml");
        // Markdown wants `**bold**` to be a distinct token from a
        // literal `*`. A two-byte rule for `**` plus a one-byte rule
        // for `*` covers both, with the multi-byte rule taking
        // precedence at a position where both could apply.
        BOOST_TEST(files::writeFileText(path,
            "escape:\n"
            "  '**': '<strong>'\n"
            "  '*': '<em>'\n")
                .has_value());

        Expected<EscapeMap> result = loadGeneratorMetadata(path);
        BOOST_TEST(result.has_value());
        if (result)
        {
            BOOST_TEST(applyEscape(*result, "**foo**") == "<strong>foo<strong>");
            BOOST_TEST(applyEscape(*result, "*bar*") == "<em>bar<em>");
            // A leftover lone `*` after a `**` match falls back to the
            // single-byte rule.
            BOOST_TEST(applyEscape(*result, "***") == "<strong><em>");
        }
    }

    void
    testLoadUtf8Codepoint()
    {
        ScopedTempDirectory td("mrdocs-addongen");
        BOOST_TEST(td);
        std::string const path =
            files::appendPath(td.path(), "g.yml");
        // 'é' is two bytes in UTF-8 (0xC3 0xA9). The whole sequence
        // becomes the multi-byte source so the replacement happens
        // as a unit instead of byte by byte.
        BOOST_TEST(files::writeFileText(path,
            "escape:\n"
            "  '\xC3\xA9': 'e'\n")
                .has_value());

        Expected<EscapeMap> result = loadGeneratorMetadata(path);
        BOOST_TEST(result.has_value());
        if (result)
        {
            BOOST_TEST(applyEscape(*result, "caf\xC3\xA9") == "cafe");
        }
    }

    void
    testLoadEmptyKey()
    {
        ScopedTempDirectory td("mrdocs-addongen");
        BOOST_TEST(td);
        std::string const path =
            files::appendPath(td.path(), "g.yml");
        BOOST_TEST(files::writeFileText(path,
            "escape:\n"
            "  '': 'x'\n")
                .has_value());

        Expected<EscapeMap> result = loadGeneratorMetadata(path);
        BOOST_TEST(!result.has_value());
    }

    void
    testLoadMissingFile()
    {
        ScopedTempDirectory td("mrdocs-addongen");
        BOOST_TEST(td);
        std::string const path =
            files::appendPath(td.path(), "does-not-exist.yml");

        Expected<EscapeMap> result = loadGeneratorMetadata(path);
        BOOST_TEST(!result.has_value());
    }

    void
    run()
    {
        testLoadEmptyFile();
        testLoadNoEscapeKey();
        testLoadValidEscape();
        testLoadNonMappingTopLevel();
        testLoadNonMappingEscape();
        testLoadMultibyteKey();
        testLoadUtf8Codepoint();
        testLoadEmptyKey();
        testLoadMissingFile();
    }
};

TEST_SUITE(
    DataDrivenGeneratorsTest,
    "clang.mrdocs.hbs.DataDrivenGenerators");

} // namespace mrdocs::hbs
