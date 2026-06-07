//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <lib/Gen/hbs/DataDrivenGenerators.hpp>
#include <lib/Gen/hbs/HandlebarsGenerator.hpp>
#include <lib/Support/Path.hpp>
#include <mrdocs/Support/Handlebars.hpp>
#include <mrdocs/Support/Path.hpp>
#include <test_suite/test_suite.hpp>
#include <fstream>
#include <string>
#include <string_view>

namespace mrdocs::hbs {

namespace {

// Write `content` verbatim to `path`. Pre-existing files are truncated.
void
writeFile(std::string_view path, std::string_view content)
{
    std::ofstream os(std::string{path}, std::ios::binary | std::ios::trunc);
    os.write(content.data(),
             static_cast<std::streamsize>(content.size()));
}

// Apply `map` to `input` and return the escaped result, so tests can
// observe an `EscapeMap`'s contents through its public surface.
std::string
applyEscape(EscapeMap const& map, std::string_view input)
{
    std::string out;
    OutputRef ref(out);
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
        writeFile(path, "");

        Expected<GeneratorManifest> result = loadGeneratorMetadata(path);
        BOOST_TEST(result.has_value());
        if (result)
        {
            // Empty map: every char passes through.
            BOOST_TEST(applyEscape(result->escape, "abc*_") == "abc*_");
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
        writeFile(path, "displayName: Markdown\n");

        Expected<GeneratorManifest> result = loadGeneratorMetadata(path);
        BOOST_TEST(result.has_value());
        if (result)
        {
            BOOST_TEST(applyEscape(result->escape, "abc") == "abc");
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
        writeFile(path,
            "escape:\n"
            "  '*': '\\*'\n"
            "  '_': '\\_'\n");

        Expected<GeneratorManifest> result = loadGeneratorMetadata(path);
        BOOST_TEST(result.has_value());
        if (result)
        {
            BOOST_TEST(applyEscape(result->escape, "*foo_bar*") == "\\*foo\\_bar\\*");
            BOOST_TEST(applyEscape(result->escape, "no specials") == "no specials");
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
        writeFile(path, "just a string\n");

        Expected<GeneratorManifest> result = loadGeneratorMetadata(path);
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
        writeFile(path, "escape: nope\n");

        Expected<GeneratorManifest> result = loadGeneratorMetadata(path);
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
        writeFile(path,
            "escape:\n"
            "  '**': '<strong>'\n"
            "  '*': '<em>'\n");

        Expected<GeneratorManifest> result = loadGeneratorMetadata(path);
        BOOST_TEST(result.has_value());
        if (result)
        {
            BOOST_TEST(applyEscape(result->escape, "**foo**") == "<strong>foo<strong>");
            BOOST_TEST(applyEscape(result->escape, "*bar*") == "<em>bar<em>");
            // A leftover lone `*` after a `**` match falls back to the
            // single-byte rule.
            BOOST_TEST(applyEscape(result->escape, "***") == "<strong><em>");
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
        writeFile(path,
            "escape:\n"
            "  '\xC3\xA9': 'e'\n");

        Expected<GeneratorManifest> result = loadGeneratorMetadata(path);
        BOOST_TEST(result.has_value());
        if (result)
        {
            BOOST_TEST(applyEscape(result->escape, "caf\xC3\xA9") == "cafe");
        }
    }

    void
    testLoadEmptyKey()
    {
        ScopedTempDirectory td("mrdocs-addongen");
        BOOST_TEST(td);
        std::string const path =
            files::appendPath(td.path(), "g.yml");
        writeFile(path,
            "escape:\n"
            "  '': 'x'\n");

        Expected<GeneratorManifest> result = loadGeneratorMetadata(path);
        BOOST_TEST(!result.has_value());
    }

    void
    testLoadMissingFile()
    {
        ScopedTempDirectory td("mrdocs-addongen");
        BOOST_TEST(td);
        std::string const path =
            files::appendPath(td.path(), "does-not-exist.yml");

        Expected<GeneratorManifest> result = loadGeneratorMetadata(path);
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
