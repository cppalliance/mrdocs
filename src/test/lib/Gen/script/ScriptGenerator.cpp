//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <lib/Gen/GeneratorManifest.hpp>
#include <lib/Gen/script/OutputSink.hpp>
#include <lib/Gen/script/ScriptGenerator.hpp>
#include <lib/Gen/script/ScriptRunner.hpp>
#include <lib/Support/Path.hpp>
#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Support/Path.hpp>
#include <mrdocs/Support/ThreadPool.hpp>
#include <test_suite/test_suite.hpp>
#include <fstream>
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

// The `config` and `params` arguments a generator receives. Tests that
// exercise only the `corpus` and `output` path pass empty objects.
dom::Value
emptyObject()
{
    return dom::Value(dom::Object());
}

// A minimal `Config` whose `object()` returns a canned DOM object. It
// lets a `build()`-level test assert what `generate` sees as `config`
// without creating a real `ConfigImpl`.
struct StubConfig
    : Config
{
    Config::Settings settings_;
    dom::Object configObject;
    mutable ThreadPool pool;

    ThreadPool&
    threadPool() const noexcept override
    {
        return pool;
    }
    Config::Settings const&
    settings() const noexcept override
    {
        return settings_;
    }
    dom::Object const&
    object() const override
    {
        return configObject;
    }
};

// An empty corpus: `build()` iterates no symbols, so it never reflects
// a real `Symbol`; it carries only the `Config` that `build()` reads
// `config` from.
struct StubCorpus
    : Corpus
{
    explicit
    StubCorpus(Config const& config)
        : Corpus(config)
    {
    }

    static Symbol const*
    noNext(Corpus const*, Symbol const*)
    {
        return nullptr;
    }
    iterator
    begin() const noexcept override
    {
        return iterator(this, nullptr, &noNext);
    }
    iterator
    end() const noexcept override
    {
        return iterator(this, nullptr, &noNext);
    }
    std::size_t
    size() const noexcept override
    {
        return 0;
    }
    Expected<Symbol const&>
    lookup(SymbolID const&, std::string_view) const override
    {
        return Unexpected(Error("stub corpus has no symbols"));
    }
    Symbol const*
    find(SymbolID const&) const noexcept override
    {
        return nullptr;
    }
    void
    qualifiedName(Symbol const&, std::string&) const override
    {
    }
    void
    qualifiedName(Symbol const&, SymbolID const&, std::string&) const override
    {
    }
};

// A two-symbol corpus shaped like what `generate(corpus, output)` sees:
// a `symbols` array whose entries carry a `name` and a flat `_id`.
dom::Value
makeCorpus()
{
    dom::Object foo;
    foo.set("name", std::string("foo"));
    foo.set("_id", std::string("0001"));
    dom::Object bar;
    bar.set("name", std::string("bar"));
    bar.set("_id", std::string("0002"));
    dom::Array symbols;
    symbols.emplace_back(dom::Value(std::move(foo)));
    symbols.emplace_back(dom::Value(std::move(bar)));
    dom::Object corpus;
    corpus.set("symbols", std::move(symbols));
    return dom::Value(std::move(corpus));
}

// A Lua generator that emits one aggregated artifact across all symbols,
// the canonical thing a per-page generator cannot produce.
constexpr std::string_view luaIndex = R"LUA(
function generate(corpus, output)
  local parts = {}
  for _, sym in ipairs(corpus.symbols) do
    parts[#parts + 1] = '{"name":"' .. sym.name .. '","id":"' .. sym._id .. '"}'
  end
  output.write("search-index.json", "[" .. table.concat(parts, ",") .. "]")
end
)LUA";

// The same generator in JavaScript, using the global-function shape.
constexpr std::string_view jsIndex = R"JS(
function generate(corpus, output) {
  var parts = [];
  for (var i = 0; i < corpus.symbols.length; i++) {
    var s = corpus.symbols[i];
    parts.push('{"name":"' + s.name + '","id":"' + s._id + '"}');
  }
  output.write("search-index.json", "[" + parts.join(",") + "]");
}
)JS";

constexpr std::string_view expectedJson =
    R"([{"name":"foo","id":"0001"},{"name":"bar","id":"0002"}])";

} // (anon)

struct ScriptGeneratorTest
{
    //
    // OutputSink
    //

    void
    testSinkWritesUnderRoot()
    {
        ScopedTempDirectory td("mrdocs-scriptgen");
        BOOST_TEST(td);
        OutputSink sink(td.path());
        // A nested relative path is created and written.
        BOOST_TEST(sink.write("a/b/out.txt", "hello").has_value());
        Expected<std::string> got =
            files::getFileText(files::appendPath(td.path(), "a", "b", "out.txt"));
        BOOST_TEST(got.has_value());
        if (got)
        {
            BOOST_TEST(*got == "hello");
        }
    }

    void
    testSinkRejectsAbsolutePath()
    {
        ScopedTempDirectory td("mrdocs-scriptgen");
        BOOST_TEST(td);
        OutputSink sink(td.path());
        // An absolute path is rejected even when it points inside root.
        std::string const abs = files::appendPath(td.path(), "x.txt");
        BOOST_TEST(!sink.write(abs, "no").has_value());
    }

    void
    testSinkRejectsEscape()
    {
        ScopedTempDirectory td("mrdocs-scriptgen");
        BOOST_TEST(td);
        OutputSink sink(td.path());
        // A path that climbs out of the output directory is rejected.
        BOOST_TEST(!sink.write("../escaped.txt", "no").has_value());
    }

    //
    // runLuaGenerator / runJsGenerator
    //

    void
    testLuaGenerator()
    {
        ScopedTempDirectory td("mrdocs-scriptgen");
        BOOST_TEST(td);
        std::string const script = files::appendPath(td.path(), "g.lua");
        writeFile(script, luaIndex);
        std::string const outDir = files::appendPath(td.path(), "out");
        OutputSink sink(outDir);

        Expected<void> result = runLuaGenerator(
            makeCorpus(), script, sink, emptyObject(), emptyObject());
        BOOST_TEST(result.has_value());
        Expected<std::string> got =
            files::getFileText(files::appendPath(outDir, "search-index.json"));
        BOOST_TEST(got.has_value());
        if (got)
        {
            BOOST_TEST(*got == expectedJson);
        }
    }

    void
    testJsGenerator()
    {
        ScopedTempDirectory td("mrdocs-scriptgen");
        BOOST_TEST(td);
        std::string const script = files::appendPath(td.path(), "g.js");
        writeFile(script, jsIndex);
        std::string const outDir = files::appendPath(td.path(), "out");
        OutputSink sink(outDir);

        Expected<void> result = runJsGenerator(
            makeCorpus(), script, sink, emptyObject(), emptyObject());
        BOOST_TEST(result.has_value());
        Expected<std::string> got =
            files::getFileText(files::appendPath(outDir, "search-index.json"));
        BOOST_TEST(got.has_value());
        if (got)
        {
            BOOST_TEST(*got == expectedJson);
        }
    }

    void
    testLuaReadsMissingFieldAsNil()
    {
        // A symbol object without a `name` field: `get("name")` yields
        // `Undefined`, which Lua must marshal as `nil` rather than abort.
        // The global namespace has no name, so a real corpus hits this.
        dom::Object noName;
        noName.set("_id", std::string("0009"));
        dom::Array symbols;
        symbols.emplace_back(dom::Value(std::move(noName)));
        dom::Object corpusObj;
        corpusObj.set("symbols", std::move(symbols));
        dom::Value const corpus(std::move(corpusObj));

        ScopedTempDirectory td("mrdocs-scriptgen");
        BOOST_TEST(td);
        std::string const script = files::appendPath(td.path(), "g.lua");
        writeFile(script, R"LUA(
function generate(corpus, output)
  local s = corpus.symbols[1]
  output.write("out.txt", "name=" .. (s.name or "NONE"))
end
)LUA");
        std::string const outDir = files::appendPath(td.path(), "out");
        OutputSink sink(outDir);

        Expected<void> result = runLuaGenerator(
            corpus, script, sink, emptyObject(), emptyObject());
        BOOST_TEST(result.has_value());
        Expected<std::string> got =
            files::getFileText(files::appendPath(outDir, "out.txt"));
        BOOST_TEST(got.has_value());
        if (got)
        {
            BOOST_TEST(*got == "name=NONE");
        }
    }

    void
    testMissingGenerateIsError()
    {
        ScopedTempDirectory td("mrdocs-scriptgen");
        BOOST_TEST(td);
        std::string const script = files::appendPath(td.path(), "empty.lua");
        writeFile(script, "-- this script defines no generate function\n");
        OutputSink sink(files::appendPath(td.path(), "out"));
        // A generator must define `generate`; its absence is an error.
        BOOST_TEST(!runLuaGenerator(
            makeCorpus(), script, sink, emptyObject(), emptyObject())
                .has_value());
    }

    void
    testLuaReceivesConfigAndParams()
    {
        ScopedTempDirectory td("mrdocs-scriptgen");
        BOOST_TEST(td);
        std::string const script = files::appendPath(td.path(), "g.lua");
        writeFile(script, R"LUA(
function generate(corpus, output, config, params)
  output.write("o.txt", tostring(config.multipage) .. "|" .. params.greeting)
end
)LUA");
        std::string const outDir = files::appendPath(td.path(), "out");
        OutputSink sink(outDir);

        dom::Object config;
        config.set("multipage", true);
        dom::Object params;
        params.set("greeting", std::string("hi"));
        Expected<void> result = runLuaGenerator(
            makeCorpus(), script, sink,
            dom::Value(std::move(config)), dom::Value(std::move(params)));
        BOOST_TEST(result.has_value());
        Expected<std::string> got =
            files::getFileText(files::appendPath(outDir, "o.txt"));
        BOOST_TEST(got.has_value());
        if (got)
        {
            BOOST_TEST(*got == "true|hi");
        }
    }

    void
    testJsReceivesConfigAndParams()
    {
        ScopedTempDirectory td("mrdocs-scriptgen");
        BOOST_TEST(td);
        std::string const script = files::appendPath(td.path(), "g.js");
        writeFile(script, R"JS(
function generate(corpus, output, config, params) {
  output.write("o.txt", String(config.multipage) + "|" + params.greeting);
}
)JS");
        std::string const outDir = files::appendPath(td.path(), "out");
        OutputSink sink(outDir);

        dom::Object config;
        config.set("multipage", true);
        dom::Object params;
        params.set("greeting", std::string("hi"));
        Expected<void> result = runJsGenerator(
            makeCorpus(), script, sink,
            dom::Value(std::move(config)), dom::Value(std::move(params)));
        BOOST_TEST(result.has_value());
        Expected<std::string> got =
            files::getFileText(files::appendPath(outDir, "o.txt"));
        BOOST_TEST(got.has_value());
        if (got)
        {
            BOOST_TEST(*got == "true|hi");
        }
    }

    // The full `build()` path: `config` comes from
    // `corpus.config.object()` and `params` from the generator's
    // manifest, both reaching the script.
    void
    testBuildPassesConfigAndParams()
    {
        ScopedTempDirectory td("mrdocs-scriptgen-build");
        BOOST_TEST(td);
        std::string const script = files::appendPath(td.path(), "g.lua");
        writeFile(script, R"LUA(
function generate(corpus, output, config, params)
  output.write("o.txt", tostring(config.multipage) .. "|" .. params.greeting)
end
)LUA");

        StubConfig config;
        config.configObject.set("multipage", true);
        StubCorpus corpus(config);

        dom::Object params;
        params.set("greeting", std::string("hi"));
        ScriptGenerator gen("build-selftest", script, std::move(params));

        std::string const outDir = files::appendPath(td.path(), "out");
        Expected<void> result = gen.build(outDir, corpus);
        BOOST_TEST(result.has_value());
        Expected<std::string> got =
            files::getFileText(files::appendPath(outDir, "o.txt"));
        BOOST_TEST(got.has_value());
        if (got)
        {
            BOOST_TEST(*got == "true|hi");
        }
    }

    //
    // discoverScriptGenerators
    //

    void
    testDiscoveryRegistersScriptGenerator()
    {
        ScopedTempDirectory td("mrdocs-scriptgen-disc");
        BOOST_TEST(td);
        // Lay out <addons>/generator/<id>/ with a script manifest. The id
        // is unusual so it does not collide with the process-global
        // registry shared across the test binary.
        std::string const id = "mrdocs-script-generator-selftest";
        std::string const genDir =
            files::appendPath(td.path(), "generator", id);
        BOOST_TEST(files::createDirectory(genDir).has_value());
        writeFile(
            files::appendPath(genDir, "mrdocs-generator.yml"),
            "script: g.lua\n");
        writeFile(files::appendPath(genDir, "g.lua"), luaIndex);

        Config::Settings settings;
        settings.addons = std::string(td.path());
        BOOST_TEST(discoverScriptGenerators(settings).has_value());
        BOOST_TEST(findGenerator(id) != nullptr);
    }

    void
    testManifestParamsParsed()
    {
        ScopedTempDirectory td("mrdocs-scriptgen-manifest");
        BOOST_TEST(td);
        std::string const yml =
            files::appendPath(td.path(), "mrdocs-generator.yml");
        writeFile(yml, "script: g.lua\nparams:\n  greeting: hi\n");
        Expected<GeneratorManifest> manifest = loadGeneratorManifest(yml);
        BOOST_TEST(manifest.has_value());
        if (manifest)
        {
            dom::Value const greeting = manifest->params.get("greeting");
            BOOST_TEST(greeting.isString());
            if (greeting.isString())
            {
                BOOST_TEST(greeting.getString().get() == "hi");
            }
        }
    }

    void
    run()
    {
        testSinkWritesUnderRoot();
        testSinkRejectsAbsolutePath();
        testSinkRejectsEscape();
        testLuaGenerator();
        testJsGenerator();
        testLuaReadsMissingFieldAsNil();
        testMissingGenerateIsError();
        testLuaReceivesConfigAndParams();
        testJsReceivesConfigAndParams();
        testBuildPassesConfigAndParams();
        testDiscoveryRegistersScriptGenerator();
        testManifestParamsParsed();
    }
};

TEST_SUITE(
    ScriptGeneratorTest,
    "clang.mrdocs.script.ScriptGenerator");

} // namespace mrdocs::script
