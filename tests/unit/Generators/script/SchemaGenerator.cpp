//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Extensions/ExtensionRegistry.hpp>
#include <mrdocs/Config.hpp>
#include <mrdocs/Config/ReferenceDirectories.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <mrdocs/Support/Filesystem/Temp.hpp>
#include <test_suite/test_suite.hpp>
#include <cstddef>
#include <fstream>
#include <ios>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace mrdocs::script {

namespace {

// Write `content` verbatim to `path`. Pre-existing files are truncated.
void
writeFile(std::string_view path, std::string_view content)
{
    std::ofstream os(std::string{path}, std::ios::binary | std::ios::trunc);
    os.write(content.data(),
             static_cast<std::streamsize>(content.size()));
}

// Count the non-overlapping occurrences of `needle` in `text`.
std::size_t
countOf(std::string_view text, std::string_view needle)
{
    std::size_t count = 0;
    std::size_t pos = text.find(needle);
    while (pos != std::string_view::npos)
    {
        ++count;
        pos = text.find(needle, pos + needle.size());
    }
    return count;
}

// One symbol kind whose fields reach types with names no other type
// shares, a type named like the fixed `Tagfile` definition, two `Text`
// types in different namespaces plus a global one, and two `Hidden`
// types whose qualified names coincide, since an anonymous namespace is
// transparent.
constexpr std::string_view corpusSource = R"CPP(
struct Text { int value; };

namespace mrdocs {

struct SymbolCommonBase {};
struct Plain { int value; };
struct Tagfile { int value; };
struct Hidden { int value; };

namespace a { struct Text { int value; }; }
namespace b { struct Text { int value; }; }

namespace {
struct Hidden { int value; };
struct Holder { Hidden hidden; };
}

struct OneSymbol : SymbolCommonBase
{
    Plain plain;
    Tagfile tagfile;
    ::Text global;
    a::Text first;
    b::Text second;
    ::mrdocs::Hidden named;
    Holder holder;
};

} // namespace mrdocs
)CPP";

// The definition names the corpus must get, one definition each.
constexpr std::string_view namedDefinitions[] = {
    "OneSymbol", "Plain", "Holder", "mrdocs.Tagfile", "a.Text", "b.Text", "Text" };

// Write the corpus source and a config with absolute paths under `srcDir`,
// and return the config's path. `addons` points back at `srcDir` only to
// satisfy the option's existence check; `addons-supplemental` names the
// addons of the MrDocs documentation, whose extensions hold schema.js.
std::string
writeInputs(std::string const& srcDir)
{
    writeFile(files::appendPath(srcDir, "input.cpp"), corpusSource);
    std::string const configPath = files::appendPath(srcDir, "mrdocs.yml");
    writeFile(configPath,
        "source-root: " + srcDir + "\n"
        "addons: " + srcDir + "\n"
        "addons-supplemental:\n  - " MRDOCS_DOCS_ADDONS_DIR "\n"
        "input:\n  - " + srcDir + "\n");
    return configPath;
}

} // (anon)

// The `schema` generator (docs/mrdocs/extensions/schema.js) names each
// definition after the type it reflects and qualifies the names that
// collide. MrDocs's own types never collide, so the committed schemas
// cannot show how a collision is resolved; this suite runs the generator
// over a corpus whose types do.
struct SchemaGeneratorTest
{
    // Built once by setup() and shared by the tests.
    std::optional<Config> config_;
    std::optional<Corpus> corpus_;
    std::optional<ExtensionRegistry> extensions_;

    void
    setup()
    {
        ScopedTempDirectory src("mrdocs-schemagen-src");
        BOOST_TEST(src);
        std::string const srcDir(src.path());
        ReferenceDirectories dirs;
        dirs.cwd = srcDir;
        dirs.mrdocsRoot = srcDir;
        Config config;
        BOOST_TEST(Config::load_file(config, writeInputs(srcDir), dirs).has_value());
        Expected<Corpus> corpus = Corpus::build(config);
        BOOST_TEST(corpus.has_value());
        if (corpus)
        {
            config_ = std::move(config);
            corpus_.emplace(std::move(corpus.value()));
            loadExtensions();
        }
    }

    // Load the extension scripts the config names, schema.js among them.
    void
    loadExtensions()
    {
        Expected<ExtensionRegistry> extensions =
            ExtensionRegistry::load(*config_);
        BOOST_TEST(extensions.has_value());
        if (extensions)
        {
            extensions_.emplace(std::move(*extensions));
        }
    }

    // Run the `schema` generator into `outDir` for the one artifact `only`
    // names (`rng` or `json`).
    bool
    runSchema(std::string const& only, std::string const& outDir)
    {
        bool ran = false;
        Generator const* gen =
            extensions_ ? extensions_->findGenerator("schema") : nullptr;
        if (gen)
        {
            Config config = *config_;
            config.output = outDir;
            dom::Object params;
            params.set("only", only);
            config.generatorOptions[std::string("schema")] = params;
            ran = gen->build(*corpus_, config).has_value();
        }
        return ran;
    }

    // The text of the artifact `file` the `schema` generator writes for
    // `only`, or an empty string if it could not run.
    std::string
    emit(std::string const& only, std::string_view file)
    {
        std::string text;
        ScopedTempDirectory td("mrdocs-schemagen");
        BOOST_TEST(td);
        std::string const outDir = files::appendPath(td.path(), "out");
        BOOST_TEST(runSchema(only, outDir));
        Expected<std::string> got =
            files::getFileText(files::appendPath(outDir, "generators", file));
        BOOST_TEST(got.has_value());
        if (got)
        {
            text = *got;
        }
        return text;
    }

    void
    testRngDefinitionNames()
    {
        std::string const rng = emit("rng", "mrdocs.rng");
        for (std::string_view name : namedDefinitions)
        {
            std::string const define =
                "<define name=\"" + std::string(name) + "\">";
            BOOST_TEST(countOf(rng, define) == 1u);
        }
        // The fixed definition keeps its name.
        BOOST_TEST(countOf(rng, "<define name=\"Tagfile\">") == 1u);
        // The two `Hidden` types keep `S_<id>`.
        BOOST_TEST(countOf(rng, "<define name=\"S_") == 2u);
    }

    void
    testJsonDefinitionNames()
    {
        std::string const json = emit("json", "mrdocs.schema.json");
        for (std::string_view name : namedDefinitions)
        {
            std::string const key = "\n    \"" + std::string(name) + "\": {";
            BOOST_TEST(countOf(json, key) == 1u);
        }
        BOOST_TEST(countOf(json, "\n    \"S_") == 2u);
    }

    void
    run()
    {
        setup();

        testRngDefinitionNames();
        testJsonDefinitionNames();
    }
};

TEST_SUITE(
    SchemaGeneratorTest,
    "clang.mrdocs.script.SchemaGenerator");

} // namespace mrdocs::script
