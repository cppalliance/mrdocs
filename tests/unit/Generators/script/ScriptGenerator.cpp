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

#include <mrdocs/Extensions/JsBinding.hpp>
#include <mrdocs/Extensions/LoadedExtensions.hpp>
#include <mrdocs/Extensions/LuaBinding.hpp>
#include <mrdocs/Generators/script/OutputSink.hpp>
#include <mrdocs/Config.hpp>
#include <mrdocs/Config/ReferenceDirectories.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <mrdocs/Support/Filesystem/Temp.hpp>
#include <test_suite/test_suite.hpp>
#include <fstream>
#include <ios>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

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

} // (anon)

// The script generator plumbing under test: `OutputSink` (the sandboxed
// file API a generator writes through), the `mrdocs.register_generator`
// loaders (`loadLuaExtensions` / `loadJsExtensions`, which turn a script
// into @ref Generator objects), and `ScriptGenerator::build` (which invokes
// a registered function with the `ctx` object and runs its whole emit). The
// generators run over a small real corpus built once for the whole suite;
// the source pulls in no headers, so extraction needs no toolchain resource
// directory.
struct ScriptGeneratorTest
{
    // Built once by setup() and shared by the generator tests. The source
    // tree is kept alive only for the build; the corpus does not reference
    // it afterwards.
    std::optional<Config> config_;
    std::optional<Corpus> corpus_;

    void
    setup()
    {
        ScopedTempDirectory src("mrdocs-scriptgen-src");
        BOOST_TEST(src);
        std::string const srcDir(src.path());
        std::string const srcFile = files::appendPath(srcDir, "input.cpp");
        writeFile(srcFile, "struct Widget {};\n");

        // A real config file with absolute paths, so no config-relative
        // placeholders need resolving. `addons` points back at this directory
        // only to satisfy the option's existence check; extraction never
        // reads it.
        std::string const configPath = files::appendPath(srcDir, "mrdocs.yml");
        writeFile(configPath,
            "source-root: " + srcDir + "\n"
            "addons: " + srcDir + "\n"
            "input:\n  - " + srcDir + "\n");

        ReferenceDirectories dirs;
        dirs.cwd = srcDir;
        dirs.mrdocsRoot = srcDir;

        Config config;
        BOOST_TEST(Config::load_file(config, configPath, dirs).has_value());

        Expected<Corpus> corpus = Corpus::build(config);
        BOOST_TEST(corpus.has_value());
        if (corpus)
        {
            config_ = std::move(config);
            corpus_.emplace(std::move(corpus.value()));
        }
    }

    // Write `src` as a `.lua`/`.js` extension under `workDir`, load it, find
    // the generator it registered under `id`, and run it over the shared
    // corpus into `outDir` with `params` as `ctx.params`. The loaded engine
    // handle is held for the whole `build`, since a script generator keeps
    // only a weak reference to its VM.
    Expected<void>
    runRegisteredGenerator(
        std::string_view lang,
        std::string_view src,
        std::string_view id,
        std::string_view workDir,
        std::string_view outDir,
        dom::Object const& params = {})
    {
        bool const isLua = lang == "lua";
        std::string const scriptPath =
            files::appendPath(workDir, isLua ? "gen.lua" : "gen.js");
        writeFile(scriptPath, src);

        Expected<LoadedExtensions> loaded = isLua
            ? loadLuaExtensions(scriptPath)
            : loadJsExtensions(scriptPath);
        if (!loaded)
        {
            return Unexpected(loaded.error());
        }

        Generator const* gen = nullptr;
        for (std::unique_ptr<Generator> const& g : loaded->generators)
        {
            if (g->id() == id)
            {
                gen = g.get();
            }
        }
        if (!gen)
        {
            return Unexpected(Error("no generator registered under this id"));
        }

        Config config = *config_;
        config.output = std::string(outDir);
        if (!params.empty())
        {
            config.generatorOptions[std::string(id)] = params;
        }
        return gen->build(*corpus_, config);
    }

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
    testSinkAppends()
    {
        ScopedTempDirectory td("mrdocs-scriptgen");
        BOOST_TEST(td);
        OutputSink sink(td.path());
        std::string const file = files::appendPath(td.path(), "streamed.txt");
        // The first write truncates; subsequent appends accumulate, so a large
        // artifact can be streamed in chunks.
        BOOST_TEST(sink.write("streamed.txt", "one").has_value());
        BOOST_TEST(sink.write("streamed.txt", "-two", true).has_value());
        BOOST_TEST(sink.write("streamed.txt", "-three", true).has_value());
        Expected<std::string> got = files::getFileText(file);
        BOOST_TEST(got.has_value());
        if (got)
        {
            BOOST_TEST(*got == "one-two-three");
        }
        // A plain (non-append) write truncates what the appends built.
        BOOST_TEST(sink.write("streamed.txt", "fresh").has_value());
        got = files::getFileText(file);
        BOOST_TEST(got.has_value());
        if (got)
        {
            BOOST_TEST(*got == "fresh");
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
    // ScriptGenerator::build
    //

    void
    testLuaGeneratorWrites()
    {
        ScopedTempDirectory td("mrdocs-scriptgen");
        BOOST_TEST(td);
        std::string const outDir = files::appendPath(td.path(), "out");
        Expected<void> ran = runRegisteredGenerator("lua", R"LUA(
mrdocs.register_generator("selftest", function(ctx)
  ctx.output.write("from-lua.txt", "hello from lua")
end)
)LUA", "selftest", td.path(), outDir);
        BOOST_TEST(ran.has_value());
        Expected<std::string> got =
            files::getFileText(files::appendPath(outDir, "from-lua.txt"));
        BOOST_TEST(got.has_value());
        if (got)
        {
            BOOST_TEST(*got == "hello from lua");
        }
    }

    void
    testJsGeneratorWrites()
    {
        ScopedTempDirectory td("mrdocs-scriptgen");
        BOOST_TEST(td);
        std::string const outDir = files::appendPath(td.path(), "out");
        Expected<void> ran = runRegisteredGenerator("js", R"JS(
mrdocs.register_generator("selftest", function(ctx) {
  ctx.output.write("from-js.txt", "hello from js");
});
)JS", "selftest", td.path(), outDir);
        BOOST_TEST(ran.has_value());
        Expected<std::string> got =
            files::getFileText(files::appendPath(outDir, "from-js.txt"));
        BOOST_TEST(got.has_value());
        if (got)
        {
            BOOST_TEST(*got == "hello from js");
        }
    }

    void
    testGeneratorReceivesConfig()
    {
        ScopedTempDirectory td("mrdocs-scriptgen");
        BOOST_TEST(td);
        std::string const outDir = files::appendPath(td.path(), "out");
        // `ctx.config` is the resolved configuration; `multipage` defaults
        // to true.
        Expected<void> ran = runRegisteredGenerator("lua", R"LUA(
mrdocs.register_generator("selftest", function(ctx)
  ctx.output.write("config.txt", tostring(ctx.config.multipage))
end)
)LUA", "selftest", td.path(), outDir);
        BOOST_TEST(ran.has_value());
        Expected<std::string> got =
            files::getFileText(files::appendPath(outDir, "config.txt"));
        BOOST_TEST(got.has_value());
        if (got)
        {
            BOOST_TEST(*got == "true");
        }
    }

    void
    testGeneratorReceivesParams()
    {
        ScopedTempDirectory td("mrdocs-scriptgen");
        BOOST_TEST(td);
        std::string const outDir = files::appendPath(td.path(), "out");
        // `ctx.params` is the generator's own `generator-options.<id>` block.
        dom::Object params;
        params.set("greeting", "hi from params");
        Expected<void> ran = runRegisteredGenerator("lua", R"LUA(
mrdocs.register_generator("selftest", function(ctx)
  ctx.output.write("params.txt", ctx.params.greeting)
end)
)LUA", "selftest", td.path(), outDir, params);
        BOOST_TEST(ran.has_value());
        Expected<std::string> got =
            files::getFileText(files::appendPath(outDir, "params.txt"));
        BOOST_TEST(got.has_value());
        if (got)
        {
            BOOST_TEST(*got == "hi from params");
        }
    }

    void
    testGeneratorIteratesCorpus()
    {
        ScopedTempDirectory td("mrdocs-scriptgen");
        BOOST_TEST(td);
        std::string const outDir = files::appendPath(td.path(), "out");
        // `ctx.corpus.symbols` is iterable; over this corpus it yields the
        // global namespace plus the extracted `Widget`.
        Expected<void> ran = runRegisteredGenerator("lua", R"LUA(
mrdocs.register_generator("selftest", function(ctx)
  local parts = {}
  for _, sym in ipairs(ctx.corpus.symbols) do
    parts[#parts + 1] = sym.name
  end
  ctx.output.write("index.txt", table.concat(parts, ","))
end)
)LUA", "selftest", td.path(), outDir);
        BOOST_TEST(ran.has_value());
        Expected<std::string> got =
            files::getFileText(files::appendPath(outDir, "index.txt"));
        BOOST_TEST(got.has_value());
        if (got)
        {
            BOOST_TEST(got->find("Widget") != std::string::npos);
        }
    }

    void
    testWriteEscapeIsError()
    {
        ScopedTempDirectory td("mrdocs-scriptgen");
        BOOST_TEST(td);
        std::string const outDir = files::appendPath(td.path(), "out");
        // A write that escapes the output directory fails in the sink, and
        // the failure surfaces back through the script as a build error.
        Expected<void> ran = runRegisteredGenerator("lua", R"LUA(
mrdocs.register_generator("selftest", function(ctx)
  ctx.output.write("../escaped.txt", "no")
end)
)LUA", "selftest", td.path(), outDir);
        BOOST_TEST(!ran.has_value());
    }

    void
    testGeneratorErrorIsReported()
    {
        ScopedTempDirectory td("mrdocs-scriptgen");
        BOOST_TEST(td);
        std::string const outDir = files::appendPath(td.path(), "out");
        // An error raised inside the generator is reported, not swallowed.
        Expected<void> ran = runRegisteredGenerator("lua", R"LUA(
mrdocs.register_generator("selftest", function(ctx)
  error("boom")
end)
)LUA", "selftest", td.path(), outDir);
        BOOST_TEST(!ran.has_value());
    }

    //
    // mrdocs.register_generator: the script bindings
    //

    void
    testRegisterGeneratorLua()
    {
        ScopedTempDirectory td("mrdocs-reggen");
        BOOST_TEST(td);
        // A Lua extension that registers a generator leaves it loadable and
        // findable by its id.
        std::string const script = files::appendPath(td.path(), "gen.lua");
        writeFile(script, "mrdocs.register_generator(\"my-gen\", function(ctx) end)\n");
        Expected<LoadedExtensions> loaded = loadLuaExtensions(script);
        BOOST_TEST(loaded.has_value());
        if (loaded)
        {
            BOOST_TEST(loaded->generators.size() == 1u);
            if (!loaded->generators.empty())
            {
                BOOST_TEST(loaded->generators.front()->id() == "my-gen");
            }
        }
    }

    void
    testRegisterGeneratorJs()
    {
        ScopedTempDirectory td("mrdocs-reggen");
        BOOST_TEST(td);
        // The JavaScript counterpart.
        std::string const script = files::appendPath(td.path(), "gen.js");
        writeFile(script, "mrdocs.register_generator(\"my-gen\", function(ctx) {});\n");
        Expected<LoadedExtensions> loaded = loadJsExtensions(script);
        BOOST_TEST(loaded.has_value());
        if (loaded)
        {
            BOOST_TEST(loaded->generators.size() == 1u);
            if (!loaded->generators.empty())
            {
                BOOST_TEST(loaded->generators.front()->id() == "my-gen");
            }
        }
    }

    void
    testRegisterNothingIsEmpty()
    {
        ScopedTempDirectory td("mrdocs-reggen");
        BOOST_TEST(td);
        // A script that registers nothing loads without error and yields no
        // generators or transforms.
        std::string const script = files::appendPath(td.path(), "gen.lua");
        writeFile(script, "local unused = 1\n");
        Expected<LoadedExtensions> loaded = loadLuaExtensions(script);
        BOOST_TEST(loaded.has_value());
        if (loaded)
        {
            BOOST_TEST(loaded->generators.empty());
            BOOST_TEST(loaded->transforms.empty());
        }
    }

    void
    run()
    {
        setup();

        testSinkWritesUnderRoot();
        testSinkAppends();
        testSinkRejectsAbsolutePath();
        testSinkRejectsEscape();

        testLuaGeneratorWrites();
        testJsGeneratorWrites();
        testGeneratorReceivesConfig();
        testGeneratorReceivesParams();
        testGeneratorIteratesCorpus();
        testWriteEscapeIsError();
        testGeneratorErrorIsReported();

        testRegisterGeneratorLua();
        testRegisterGeneratorJs();
        testRegisterNothingIsEmpty();
    }
};

TEST_SUITE(
    ScriptGeneratorTest,
    "clang.mrdocs.script.ScriptGenerator");

} // namespace mrdocs::script
