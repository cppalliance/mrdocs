//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <lib/ConfigImpl.hpp>
#include <lib/CorpusImpl.hpp>
#include <lib/Extensions/JsBinding.hpp>
#include <lib/Extensions/LuaBinding.hpp>
#include <lib/Gen/script/OutputSink.hpp>
#include <lib/Gen/script/ScriptGenerator.hpp>
#include <lib/Support/Path.hpp>
#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/JavaScript.hpp>
#include <mrdocs/Support/Lua.hpp>
#include <mrdocs/Support/Path.hpp>
#include <mrdocs/Support/ThreadPool.hpp>
#include <test_suite/test_suite.hpp>
#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace mrdocs::script {

namespace {

// A minimal `Config` whose `object()` returns a canned DOM object, so a
// test can assert what a generator sees as `ctx.config` without building
// a real `ConfigImpl`.
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

// An empty corpus: the runner iterates no symbols, so `ctx.corpus.symbols`
// is an empty array. It carries only the `Config` the runner reads
// `ctx.config` from.
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

// Load `src`, which must define a global `generate(ctx)`, and return it as
// a callable `dom::Function`. The function is self-owning: it anchors the
// chunk in the Lua registry and carries a copy of the context, so it
// outlives the local VM here exactly as a `mrdocs.register_generator` function
// outlives the extension that declared it.
dom::Function
makeLuaGenerator(std::string_view src)
{
    lua::Context ctx;
    lua::Scope scope(ctx);
    Expected<lua::Function> chunk =
        scope.loadChunk(std::string(src), "generator.lua");
    BOOST_TEST(chunk.has_value());
    if (chunk)
    {
        BOOST_TEST(chunk->call().has_value());
    }
    lua_State* L = static_cast<lua_State*>(ctx.nativeState());
    lua_getglobal(L, "generate");
    int const ref = luaL_ref(L, LUA_REGISTRYINDEX);
    return lua::makeCallable(ctx, ref);
}

// The JavaScript counterpart of `makeLuaGenerator`. A JS function holds
// only a weak reference to its interpreter, so - exactly as a corpus does
// for a `mrdocs.register_generator` function - the caller must keep the VM
// alive for as long as it intends to call the generator.
// `JsGenerator::keepAlive` does that; dropping it tears the interpreter
// down. (A Lua callable instead
// carries its own VM, so `makeLuaGenerator` needs no such companion.)
struct JsGenerator
{
    dom::Function generate;
    js::Context keepAlive;
};

JsGenerator
makeJsGenerator(std::string_view src)
{
    js::Context ctx;
    js::Scope scope(ctx);
    BOOST_TEST(scope.script(std::string(src)).has_value());
    Expected<js::Value> fn = scope.getGlobal("generate");
    BOOST_TEST(fn.has_value());
    return JsGenerator{fn->getFunction(), ctx};
}

// Write `content` verbatim to `path`. Pre-existing files are truncated.
void
writeFile(std::string_view path, std::string_view content)
{
    std::ofstream os(std::string{path}, std::ios::binary | std::ios::trunc);
    os.write(content.data(),
             static_cast<std::streamsize>(content.size()));
}

// A generator function that ignores its argument and returns `value`, so two
// registrations can be told apart by what the resolved one returns.
dom::Function
makeConstGenerator(std::int64_t value)
{
    return dom::makeVariadicInvocable(
        [value](dom::Array const&) -> Expected<dom::Value, Error>
        {
            return dom::Value(value);
        });
}

// An empty in-memory configuration. The ThreadPool is stored by reference,
// so the caller must keep it alive at least as long as the config.
std::shared_ptr<ConfigImpl const>
makeConfig(ThreadPool& pool)
{
    return std::make_shared<ConfigImpl>(ConfigImpl::access_token{}, pool);
}

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
    // runScriptGenerator
    //

    // Run `generate` over an empty stub corpus, writing into `outDir`, and
    // return whether it succeeded.
    static Expected<void>
    runOver(
        dom::Function const& generate,
        std::string_view outDir)
    {
        StubConfig config;
        config.configObject.set("multipage", true);
        StubCorpus corpus(config);
        return runScriptGenerator(generate, "selftest", corpus, outDir);
    }

    void
    testLuaGeneratorWrites()
    {
        ScopedTempDirectory td("mrdocs-scriptgen");
        BOOST_TEST(td);
        std::string const outDir = files::appendPath(td.path(), "out");
        dom::Function gen = makeLuaGenerator(R"LUA(
function generate(ctx)
  ctx.output.write("from-lua.txt", "hello from lua")
end
)LUA");
        BOOST_TEST(runOver(gen, outDir).has_value());
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
        JsGenerator gen = makeJsGenerator(R"JS(
function generate(ctx) {
  ctx.output.write("from-js.txt", "hello from js");
}
)JS");
        BOOST_TEST(runOver(gen.generate, outDir).has_value());
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
        // `ctx.config` is the resolved configuration; `runOver` sets
        // `multipage` to true on it.
        dom::Function gen = makeLuaGenerator(R"LUA(
function generate(ctx)
  ctx.output.write("config.txt", tostring(ctx.config.multipage))
end
)LUA");
        BOOST_TEST(runOver(gen, outDir).has_value());
        Expected<std::string> got =
            files::getFileText(files::appendPath(outDir, "config.txt"));
        BOOST_TEST(got.has_value());
        if (got)
        {
            BOOST_TEST(*got == "true");
        }
    }

    void
    testGeneratorIteratesCorpus()
    {
        ScopedTempDirectory td("mrdocs-scriptgen");
        BOOST_TEST(td);
        std::string const outDir = files::appendPath(td.path(), "out");
        // `ctx.corpus.symbols` is iterable; over the empty stub corpus it
        // yields nothing, so the aggregated artifact is an empty list.
        dom::Function gen = makeLuaGenerator(R"LUA(
function generate(ctx)
  local parts = {}
  for _, sym in ipairs(ctx.corpus.symbols) do
    parts[#parts + 1] = sym.name
  end
  ctx.output.write("index.txt", "[" .. table.concat(parts, ",") .. "]")
end
)LUA");
        BOOST_TEST(runOver(gen, outDir).has_value());
        Expected<std::string> got =
            files::getFileText(files::appendPath(outDir, "index.txt"));
        BOOST_TEST(got.has_value());
        if (got)
        {
            BOOST_TEST(*got == "[]");
        }
    }

    void
    testWriteEscapeIsError()
    {
        ScopedTempDirectory td("mrdocs-scriptgen");
        BOOST_TEST(td);
        std::string const outDir = files::appendPath(td.path(), "out");
        // A write that escapes the output directory fails in the sink, and
        // the failure surfaces back through the script as a runner error.
        dom::Function gen = makeLuaGenerator(R"LUA(
function generate(ctx)
  ctx.output.write("../escaped.txt", "no")
end
)LUA");
        BOOST_TEST(!runOver(gen, outDir).has_value());
    }

    void
    testGeneratorErrorIsReported()
    {
        ScopedTempDirectory td("mrdocs-scriptgen");
        BOOST_TEST(td);
        std::string const outDir = files::appendPath(td.path(), "out");
        // An error raised inside the generator is reported, not swallowed.
        dom::Function gen = makeLuaGenerator(R"LUA(
function generate(ctx)
  error("boom")
end
)LUA");
        BOOST_TEST(!runOver(gen, outDir).has_value());
    }

    //
    // mrdocs.register_generator: corpus host and the script bindings
    //

    void
    testHostKeepsFirstRegistration()
    {
        ThreadPool pool;
        CorpusImpl corpus(makeConfig(pool));

        // The first registration of an id wins; a later one is ignored.
        corpus.registerScriptGenerator("a", makeConstGenerator(1));
        corpus.registerScriptGenerator("a", makeConstGenerator(2));

        BOOST_TEST(corpus.findScriptGenerator("missing") == nullptr);
        dom::Function const* found = corpus.findScriptGenerator("a");
        BOOST_TEST(found != nullptr);
        if (found)
        {
            Expected<dom::Value> got = found->try_invoke(dom::Value());
            BOOST_TEST(got.has_value());
            if (got)
            {
                BOOST_TEST(got->getInteger() == 1);
            }
        }
    }

    void
    testRegisterGeneratorLua()
    {
        ThreadPool pool;
        CorpusImpl corpus(makeConfig(pool));
        ScopedTempDirectory td("mrdocs-reggen");
        BOOST_TEST(td);
        // A Lua extension that registers a generator leaves it findable on
        // the corpus by its id, and does not warn about registering nothing.
        std::string const script = files::appendPath(td.path(), "gen.lua");
        writeFile(script, "mrdocs.register_generator(\"my-gen\", function(ctx) end)\n");
        BOOST_TEST(runOneLuaExtension(corpus, script).has_value());
        BOOST_TEST(corpus.findScriptGenerator("my-gen") != nullptr);
    }

    void
    testRegisterGeneratorJs()
    {
        ThreadPool pool;
        CorpusImpl corpus(makeConfig(pool));
        ScopedTempDirectory td("mrdocs-reggen");
        BOOST_TEST(td);
        // The JavaScript counterpart.
        std::string const script = files::appendPath(td.path(), "gen.js");
        writeFile(script, "mrdocs.register_generator(\"my-gen\", function(ctx) {});\n");
        BOOST_TEST(runOneJsExtension(corpus, script).has_value());
        BOOST_TEST(corpus.findScriptGenerator("my-gen") != nullptr);
    }

    void
    run()
    {
        testSinkWritesUnderRoot();
        testSinkRejectsAbsolutePath();
        testSinkRejectsEscape();
        testLuaGeneratorWrites();
        testJsGeneratorWrites();
        testGeneratorReceivesConfig();
        testGeneratorIteratesCorpus();
        testWriteEscapeIsError();
        testGeneratorErrorIsReported();
        testHostKeepsFirstRegistration();
        testRegisterGeneratorLua();
        testRegisterGeneratorJs();
    }
};

TEST_SUITE(
    ScriptGeneratorTest,
    "clang.mrdocs.script.ScriptGenerator");

} // namespace mrdocs::script
