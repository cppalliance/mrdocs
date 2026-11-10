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

#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Extensions/ExtensionRegistry.hpp>
#include <mrdocs/Extensions/TransformExtensionContext.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Session.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <mrdocs/Support/Filesystem/Temp.hpp>
#include <mrdocs/Support/Reporter.hpp>
#include <mrdocs/Transform.hpp>
#include <test_suite/test_suite.hpp>
#include "../TestProject.hpp"
#include <algorithm>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mrdocs {

namespace {

// A script transform that appends the `suffix` of its own options to the name
// of the Widget.
constexpr std::string_view suffixerLua = R"lua(mrdocs.register_transform("suffixer", function(ctx)
    for _, sym in ipairs(ctx.corpus.symbols) do
        if sym.name == "Widget" then
            sym.name = sym.name .. ctx.params.suffix
        end
    end
end)
)lua";

// Writes an empty table to an object field and to an array field of a symbol
// without documentation, then fills the object. It only acts when it is given
// a `doc` option, so the other tests run as if it were not there.
constexpr std::string_view emptyTablesLua = R"lua(mrdocs.register_transform("empty-tables", function(ctx)
    if not ctx.params.doc then return end
    for _, sym in ipairs(ctx.corpus.symbols) do
        if sym.name == "Widget" then
            sym.doc = {}
            sym.doc.brief = "A widget."
            sym.doc.params = {}
        end
    end
end)
)lua";

constexpr std::string_view reportLua = R"lua(mrdocs.register_transform("report", function(ctx)
    ctx.report.warn("Hello from a transform")
end)
)lua";

constexpr std::string_view reportJs = R"js(mrdocs.register_transform("report", function(ctx) {
    ctx.report.warn("Hello from a transform");
});
)js";

// Keeps every symbol in a global until the engine closes. The finalizer of
// the kept value writes the names to `@READ@`, and the transform records the
// count in `@COUNTED@`.
constexpr std::string_view keeperLua = R"lua(local keeper
mrdocs.register_transform("keeper", function(ctx)
    local symbols = ctx.corpus.symbols
    local kept = {}
    for i = 1, #symbols do kept[i] = symbols[i] end
    local file = io.open([==[@COUNTED@]==], "w")
    file:write(#kept, "\n")
    file:close()
    keeper = setmetatable({ symbols = kept }, {
        __gc = function(self)
            local out = io.open([==[@READ@]==], "w")
            for _, symbol in ipairs(self.symbols) do
                out:write(tostring(symbol.name), "\n")
            end
            out:close()
        end
    })
end)
)lua";

// `text` with each `@KEY@` of `values` replaced by its value.
std::string
substitute(
    std::string_view text,
    std::initializer_list<std::pair<std::string_view, std::string_view>> values)
{
    std::string result(text);
    for (auto const& [key, value] : values)
    {
        std::string const marker = "@" + std::string(key) + "@";
        BOOST_TEST(result.find(marker) != std::string::npos);
        for (std::size_t at = result.find(marker); at != std::string::npos;
            at = result.find(marker, at + value.size()))
        {
            result.replace(at, marker.size(), value);
        }
    }
    return result;
}

// The name of the symbol that `corpus` finds for `name`, or "none". Only the
// lookup of a corpus that is not const remembers its results, so that is the
// one used.
std::string
lookedUp(Corpus& corpus, std::string_view name)
{
    Expected<Symbol const&> const found =
        corpus.lookup(SymbolID::global, name);
    return found ? found->Name : std::string("none");
}

// A transform that runs `body` with the corpus and its params, and fails
// when `fails` says so. A test checks what `body` records.
class ProbeTransform final
    : public Transform
{
public:
    using Body = std::function<void(Corpus&, dom::Object const&)>;

private:
    std::string id_;
    Body body_;
    bool fails_;

public:
    ProbeTransform(std::string id, Body body, bool fails = false)
        : id_(std::move(id))
        , body_(std::move(body))
        , fails_(fails)
    {
    }

    std::string_view
    id() const noexcept override
    {
        return id_;
    }

    Expected<void>
    apply(Corpus& corpus, Config const& config, report::Reporter&) const override
    {
        body_(corpus, getTransformParams(*this, config));
        Expected<void> result;
        if (fails_)
        {
            result = Unexpected(Error("deliberate failure"));
        }
        return result;
    }
};

// A transform that appends its id to `log` when it runs, and fails when
// `fails` says so.
std::shared_ptr<Transform const>
loggingTransform(
    std::string id,
    std::vector<std::string>& log,
    bool fails = false)
{
    return std::make_shared<ProbeTransform>(
        id,
        [id, &log](Corpus&, dom::Object const&)
        { log.push_back(id); },
        fails);
}

// A transform that appends "<id>:<tag>:<Widget>" to `log`: its own id, the
// `tag` of its params, and the name the corpus gives the symbol that was
// declared as `Widget`, which an earlier transform may have renamed.
std::shared_ptr<Transform const>
reportingTransform(std::string id, std::vector<std::string>& log)
{
    return std::make_shared<ProbeTransform>(
        id,
        [id, &log](Corpus& corpus, dom::Object const& params)
        {
            std::string tag = "no-tag";
            if (params.exists("tag"))
            {
                tag = std::string(params.get("tag").getString());
            }
            std::string widget = "no-widget";
            for (Symbol const& symbol : corpus)
            {
                if (symbol.Name.starts_with("Widget") ||
                    symbol.Name.starts_with("Gadget"))
                {
                    widget = symbol.Name;
                }
            }
            log.push_back(id + ":" + tag + ":" + widget);
        });
}

// A transform that renames the symbol named `from` to `to`.
std::shared_ptr<Transform const>
renamingTransform(std::string from, std::string to)
{
    return std::make_shared<ProbeTransform>(
        "renamer",
        [from, to](Corpus& corpus, dom::Object const&)
        {
            Expected<Symbol const&> const found =
                corpus.lookup(SymbolID::global, from);
            if (found)
            {
                corpus.find(found->id)->Name = to;
            }
        });
}

} // (anon)

// The pipeline `ExtensionRegistry::applyTransforms` runs, over a small real
// corpus. The source pulls in no headers, so extraction needs no toolchain
// resource directory.
struct ExtensionRegistryTest
{
    // The reporter the configurations, corpora and extensions of the test
    // report to, declared first so that it outlives them.
    mutable report::Reporter reporter_;

    std::optional<TestProject> project_;
    Config const* config_ = nullptr;

    // Build the corpus of the sample project. Tests that change symbols
    // build their own, so that nothing carries over between them.
    Expected<Corpus>
    buildCorpus() const
    {
        return Corpus::build(*config_, reporter_);
    }

    // Build a project whose addon root holds `scripts`, each a file name and
    // its text.
    void
    setup(std::initializer_list<std::pair<std::string_view, std::string_view>> scripts)
    {
        project_.emplace(makeTestProject(
            "mrdocs-pipeline", scripts, reporter_));
        config_ = &project_->config;
    }

    void
    testAddOrder()
    {
        Expected<Corpus> corpus = buildCorpus();
        BOOST_TEST(corpus.has_value());
        if (!corpus)
        {
            return;
        }
        std::vector<std::string> log;
        ExtensionRegistry registry;
        for (std::string_view id : {"first", "second", "third"})
        {
            BOOST_TEST(registry.installTransform(
                                   loggingTransform(std::string(id), log))
                    .has_value());
        }
        BOOST_TEST(registry.applyTransforms(*corpus, *config_, reporter_).has_value());
        BOOST_TEST((log == std::vector<std::string>{"first", "second", "third"}));
    }

    void
    testSameTransformIsAddedOnce()
    {
        Expected<Corpus> corpus = buildCorpus();
        BOOST_TEST(corpus.has_value());
        if (!corpus)
        {
            return;
        }
        std::vector<std::string> log;
        std::shared_ptr<Transform const> const first =
            loggingTransform("first", log);
        ExtensionRegistry registry;
        BOOST_TEST(registry.installTransform(first).has_value());
        BOOST_TEST(registry.installTransform(loggingTransform("second", log))
                .has_value());
        BOOST_TEST(registry.installTransform(first).has_value());
        BOOST_TEST(!registry.installTransform(nullptr).has_value());
        BOOST_TEST(registry.applyTransforms(*corpus, *config_, reporter_).has_value());
        BOOST_TEST((log == std::vector<std::string>{"first", "second"}));
    }

    void
    testStopsAtFirstError()
    {
        Expected<Corpus> corpus = buildCorpus();
        BOOST_TEST(corpus.has_value());
        if (!corpus)
        {
            return;
        }
        std::vector<std::string> log;
        ExtensionRegistry registry;
        BOOST_TEST(registry.installTransform(
                               loggingTransform("first", log))
                .has_value());
        BOOST_TEST(registry.installTransform(
                               loggingTransform("second", log, true))
                .has_value());
        BOOST_TEST(registry.installTransform(
                               loggingTransform("third", log))
                .has_value());
        Expected<void> const result =
            registry.applyTransforms(*corpus, *config_, reporter_);
        BOOST_TEST(!result.has_value());
        BOOST_TEST((log == std::vector<std::string>{"first", "second"}));
        if (!result)
        {
            BOOST_TEST(result.error().reason() ==
                "the transform \"second\" failed: deliberate failure");
        }
    }

    // A transform added before the scripts, one registered by a script, and
    // one added after, run in that order, each with the options of its own
    // id. The script renames the Widget between the two native transforms,
    // so the second one sees the new name only if the script ran before it.
    void
    testScriptsAndNativeInLoadOrder()
    {
        Expected<Corpus> corpus = buildCorpus();
        BOOST_TEST(corpus.has_value());
        if (!corpus)
        {
            return;
        }
        Config config = *config_;
        auto options = [](std::string_view key, std::string_view value)
        {
            dom::Object object;
            object.set(key, value);
            return object;
        };
        config.transformOptions["before"] = options("tag", "a");
        config.transformOptions["suffixer"] = options("suffix", "-s");
        config.transformOptions["after"] = options("tag", "b");

        std::vector<std::string> log;
        ExtensionRegistry registry;
        BOOST_TEST(registry.installTransform(
                               reportingTransform("before", log))
                .has_value());
        BOOST_TEST(registry.loadExtensions(config, reporter_).has_value());
        BOOST_TEST(registry.installTransform(
                               reportingTransform("after", log))
                .has_value());
        BOOST_TEST(registry.applyTransforms(*corpus, config, reporter_).has_value());
        BOOST_TEST((log == std::vector<std::string>{"before:a:Widget", "after:b:Widget-s"}));
    }

    // Loading the scripts a second time adds nothing: the suffix is appended
    // once, not twice.
    void
    testScriptsAreLoadedOnce()
    {
        Expected<Corpus> corpus = buildCorpus();
        BOOST_TEST(corpus.has_value());
        if (!corpus)
        {
            return;
        }
        Config config = *config_;
        dom::Object suffix;
        suffix.set("suffix", "-s");
        config.transformOptions["suffixer"] = suffix;
        dom::Object tag;
        tag.set("tag", "a");
        config.transformOptions["after"] = tag;

        std::vector<std::string> log;
        ExtensionRegistry registry;
        BOOST_TEST(registry.loadExtensions(config, reporter_).has_value());
        BOOST_TEST(registry.loadExtensions(config, reporter_).has_value());
        BOOST_TEST(registry.installTransform(
                               reportingTransform("after", log))
                .has_value());
        BOOST_TEST(registry.applyTransforms(*corpus, config, reporter_).has_value());
        BOOST_TEST((log == std::vector<std::string>{"after:a:Widget-s"}));
    }

    // A transform without a block of options gets an empty object.
    void
    testMissingParamsAreEmpty()
    {
        Expected<Corpus> corpus = buildCorpus();
        BOOST_TEST(corpus.has_value());
        if (!corpus)
        {
            return;
        }
        std::vector<std::string> log;
        ExtensionRegistry registry;
        BOOST_TEST(registry.installTransform(
                               reportingTransform("alone", log))
                .has_value());
        BOOST_TEST(registry.applyTransforms(*corpus, *config_, reporter_).has_value());
        BOOST_TEST((log == std::vector<std::string>{"alone:no-tag:Widget"}));
    }

    // A lookup by name is remembered, and a rename makes what was
    // remembered wrong. The pipeline forgets it after each transform, so
    // the next transform and the generators see the new name.
    void
    testLookupCacheSeesRename()
    {
        Expected<Corpus> corpus = buildCorpus();
        BOOST_TEST(corpus.has_value());
        if (!corpus)
        {
            return;
        }
        BOOST_TEST(lookedUp(*corpus, "Widget") == "Widget");
        BOOST_TEST(lookedUp(*corpus, "Gadget") == "none");

        std::vector<std::string> seen;
        ExtensionRegistry registry;
        BOOST_TEST(registry.installTransform(
                               renamingTransform("Widget", "Gadget"))
                .has_value());
        BOOST_TEST(registry.installTransform(
                               std::make_shared<ProbeTransform>(
                                   "observer",
                                   [&seen](Corpus& c, dom::Object const&)
                                   {
                                       seen.push_back(lookedUp(c, "Widget"));
                                       seen.push_back(lookedUp(c, "Gadget"));
                                   }))
                .has_value());
        BOOST_TEST(registry.applyTransforms(*corpus, *config_, reporter_).has_value());
        BOOST_TEST((seen == std::vector<std::string>{"none", "Gadget"}));
        BOOST_TEST(lookedUp(*corpus, "Widget") == "none");
        BOOST_TEST(lookedUp(*corpus, "Gadget") == "Gadget");
    }

    // The lookups are forgotten when a transform fails as well.
    void
    testLookupCacheIsClearedOnFailure()
    {
        Expected<Corpus> corpus = buildCorpus();
        BOOST_TEST(corpus.has_value());
        if (!corpus)
        {
            return;
        }
        BOOST_TEST(lookedUp(*corpus, "Gadget") == "none");
        ExtensionRegistry registry;
        BOOST_TEST(registry.installTransform(
                               std::make_shared<ProbeTransform>(
                                   "renames-then-fails",
                                   [](Corpus& c, dom::Object const&)
                                   {
                                       Expected<Symbol const&> const found =
                                           c.lookup(SymbolID::global, "Widget");
                                       if (found)
                                       {
                                           c.find(found->id)->Name = "Gadget";
                                       }
                                   },
                                   true))
                .has_value());
        BOOST_TEST(!registry.applyTransforms(*corpus, *config_, reporter_).has_value());
        BOOST_TEST(lookedUp(*corpus, "Gadget") == "Gadget");
    }

    // `{}` is both an empty array and an empty object in Lua. Assigned to a
    // field that expects an object (`doc` of an undocumented symbol), it is
    // an empty object, so the script can fill it in afterwards. Assigned to
    // a field that expects an array (`doc.params`), it is an empty array.
    void
    testEmptyTableTakesKindOfField()
    {
        Expected<Corpus> corpus = buildCorpus();
        BOOST_TEST(corpus.has_value());
        if (!corpus)
        {
            return;
        }
        Config config = *config_;
        dom::Object flag;
        flag.set("doc", true);
        config.transformOptions["empty-tables"] = flag;
        dom::Object suffix;
        suffix.set("suffix", "");
        config.transformOptions["suffixer"] = suffix;

        ExtensionRegistry registry;
        BOOST_TEST(registry.loadExtensions(config, reporter_).has_value());
        Expected<void> const result =
            registry.applyTransforms(*corpus, config, reporter_);
        BOOST_TEST(result.has_value());
        if (!result)
        {
            return;
        }
        bool found = false;
        for (Symbol const& symbol : *corpus)
        {
            if (symbol.Name == "Widget")
            {
                found = true;
                BOOST_TEST(symbol.doc.has_value());
                if (symbol.doc)
                {
                    BOOST_TEST(symbol.doc->brief.has_value());
                    BOOST_TEST(symbol.doc->params.empty());
                }
            }
        }
        BOOST_TEST(found);
    }

    // `ctx.report.warn` in a script reports through the reporter of the
    // session that runs it: the warning is counted there, and under
    // `warn-as-error` it is counted as an error, which fails the run.
    void
    testScriptReport()
    {
        for (bool const lua : {true, false})
        {
            for (bool const warnAsError : {false, true})
            {
                setup({{lua ? "report.lua" : "report.js", lua ? reportLua : reportJs}});
                Config config = *config_;
                config.warnAsError = warnAsError;
                std::optional<Corpus> corpus;
                Expected<Session> loaded = Session::create(config);
                BOOST_TEST(loaded.has_value());
                if (!loaded)
                {
                    return;
                }
                Session& session = *loaded;
                session.log().setMinimumLevel(report::Level::fatal);
                Expected<Corpus> built = Corpus::build(config, session.log());
                BOOST_TEST(built.has_value());
                if (!built)
                {
                    return;
                }
                corpus.emplace(std::move(*built));
                report::Results const before = session.log().results();
                auto const fixtureErrors = reporter_.results().errorCount;
                BOOST_TEST(session.applyTransforms(*corpus).has_value());
                report::Results const after = session.log().results();
                BOOST_TEST(after.warnCount - before.warnCount ==
                    (warnAsError ? 0u : 1u));
                BOOST_TEST(after.errorCount - before.errorCount ==
                    (warnAsError ? 1u : 0u));
                // The reporter of the test fixture saw none of it
                BOOST_TEST(reporter_.results().errorCount == fixtureErrors);
            }
        }
    }

    // A transform reports through the session that applies it, not the one
    // whose reporter built the corpus.
    void
    testTransformReportsToRunningSession()
    {
        setup({{"report.lua", reportLua}});
        std::optional<Corpus> corpus;
        Expected<Session> loadedBuilder = Session::create(*config_);
        Expected<Session> loadedRunner = Session::create(*config_);
        BOOST_TEST(loadedBuilder.has_value());
        BOOST_TEST(loadedRunner.has_value());
        if (!loadedBuilder || !loadedRunner)
        {
            return;
        }
        Session& builder = *loadedBuilder;
        Session& runner = *loadedRunner;
        builder.log().setMinimumLevel(report::Level::fatal);
        runner.log().setMinimumLevel(report::Level::fatal);
        Expected<Corpus> built = Corpus::build(*config_, builder.log());
        BOOST_TEST(built.has_value());
        if (!built)
        {
            return;
        }
        corpus.emplace(std::move(*built));
        report::Results const builderBefore = builder.log().results();
        report::Results const runnerBefore = runner.log().results();
        BOOST_TEST(runner.applyTransforms(*corpus).has_value());
        BOOST_TEST(runner.log().results().warnCount ==
            runnerBefore.warnCount + 1u);
        BOOST_TEST(builder.log().results().warnCount ==
            builderBefore.warnCount);
    }

    // A script may keep symbols until its engine closes, which is after the
    // run: the finalizer of a kept value reads them then. The session is
    // closed here while the corpus is alive, so the names the finalizer
    // wrote are compared with the corpus's own. A corpus freed before the
    // engine closed would make them differ.
    void
    testCorpusOutlivesEngine()
    {
        ScopedTempDirectory records("mrdocs-ctx-records");
        BOOST_TEST(records);
        std::string const counted = files::appendPath(records.path(), "counted.txt");
        std::string const read = files::appendPath(records.path(), "read.txt");
        std::string const keeper = substitute(keeperLua,
            {{"COUNTED", counted}, {"READ", read}});
        setup({{"keeper.lua", keeper}});
        Expected<Session> loaded = Session::create(*config_);
        BOOST_TEST(loaded.has_value());
        if (!loaded)
        {
            return;
        }
        std::optional<Session> session(std::move(*loaded));
        session->log().setMinimumLevel(report::Level::fatal);
        Expected<Corpus> corpus = Corpus::build(*config_, session->log());
        BOOST_TEST(corpus.has_value());
        if (!corpus)
        {
            return;
        }
        BOOST_TEST(session->applyTransforms(*corpus).has_value());
        // The run is over, and the engine is still open
        BOOST_TEST(files::getFileText(counted).has_value());
        BOOST_TEST(!files::getFileText(read).has_value());
        session.reset();
        // Closing the session ran the finalizer, which read every name
        Expected<std::string> const readText = files::getFileText(read);
        BOOST_TEST(readText.has_value());
        if (!readText)
        {
            return;
        }
        std::vector<std::string> expected;
        for (Symbol const& symbol : *corpus)
        {
            // The global namespace has no name, which a script sees as nil
            expected.push_back(
                symbol.Name.empty() ? "nil\n" : symbol.Name + "\n");
        }
        std::vector<std::string> actual;
        std::size_t start = 0;
        for (std::size_t end; (end = readText->find('\n', start)) != std::string::npos;
            start = end + 1)
        {
            actual.push_back(readText->substr(start, end + 1 - start));
        }
        std::sort(expected.begin(), expected.end());
        std::sort(actual.begin(), actual.end());
        BOOST_TEST(!actual.empty());
        BOOST_TEST(actual == expected);
    }

    void
    run()
    {
        setup({{"suffixer.lua", suffixerLua},
            {"empty_tables.lua", emptyTablesLua}});
        if (config_)
        {
            testAddOrder();
            testSameTransformIsAddedOnce();
            testStopsAtFirstError();
            testScriptsAndNativeInLoadOrder();
            testScriptsAreLoadedOnce();
            testMissingParamsAreEmpty();
            testLookupCacheSeesRename();
            testLookupCacheIsClearedOnFailure();
            testEmptyTableTakesKindOfField();
        }
        testCorpusOutlivesEngine();
        testScriptReport();
        testTransformReportsToRunningSession();
    }
};

TEST_SUITE(
    ExtensionRegistryTest,
    "clang.mrdocs.Extensions.ExtensionRegistry");

} // mrdocs
