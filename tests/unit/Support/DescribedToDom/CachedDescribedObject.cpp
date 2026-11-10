//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Dom/OverlayObject.hpp>
#include <mrdocs/Extensions/ExtensionRegistry.hpp>
#include <mrdocs/Extensions/GeneratorExtensionContext.hpp>
#include <mrdocs/Extensions/TransformExtensionContext.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Support/DescribedToDom/CachedDescribedObject.hpp>
#include <mrdocs/Support/Reporter.hpp>
#include <mrdocs/Support/Report/ReportDom.hpp>
#include <mrdocs/Transform.hpp>
#include <test_suite/test_suite.hpp>
#include "../../TestProject.hpp"
#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mrdocs {

namespace {

// A transform and a generator that only name the context they stand for.
class NamedTransform final
    : public Transform
{
public:
    std::string_view
    id() const noexcept override
    {
        return "named";
    }

    Expected<void>
    apply(Corpus&, Config const&, report::Reporter&) const override
    {
        return {};
    }
};

class NamedGenerator final
    : public Generator
{
public:
    std::string_view
    id() const noexcept override
    {
        return "named";
    }

    std::string_view
    displayName() const noexcept override
    {
        return "Named";
    }

    std::string_view
    fileExtension() const noexcept override
    {
        return "txt";
    }

    Expected<void>
    build(Corpus const&, Config const&, report::Reporter&) const override
    {
        return {};
    }
};

// Two transforms: `saver` keeps its `ctx` in a global, and `reader` renames
// `Widget` to the `tag` of the saved `ctx.params`, which it can read only if
// the kept `ctx` is still alive.
constexpr std::string_view keepLua = R"lua(mrdocs.register_transform("saver", function(ctx)
    saved = ctx
end)
mrdocs.register_transform("reader", function(ctx)
    local tag = saved.params.tag .. saved.config.output:sub(1, 0)
    for _, sym in ipairs(ctx.corpus.symbols) do
        if sym.name == "Widget" then sym.name = tag end
    end
end)
)lua";

constexpr std::string_view keepJs = R"js(var saved;
mrdocs.register_transform("saver", function(ctx) { saved = ctx; });
mrdocs.register_transform("reader", function(ctx) {
    var tag = saved.params.tag + saved.config.output.slice(0, 0);
    for (var i = 0; i < ctx.corpus.symbols.length; ++i) {
        var sym = ctx.corpus.symbols[i];
        if (sym.name === "Widget") { sym.name = tag; }
    }
});
)js";

} // (anon)

// The object a script receives as `ctx`: the keys added by hand overlaid on a
// described context, in Lua and in JavaScript.
struct CachedDescribedObjectTest
{
    // The reporter the configurations, corpora and extensions of the test
    // report to, declared first so that it outlives them.
    report::Reporter reporter_;

    std::optional<TestProject> project_;
    Config const* config_ = nullptr;

    void
    setup(std::string_view scriptName, std::string_view script)
    {
        project_.emplace(makeTestProject(
            "mrdocs-ctx", {{scriptName, script}}, reporter_));
        config_ = &project_->config;
    }

    // The object has no `$meta` key: it is not listed, it does not exist and
    // reading it gives an undefined value. The other keys stay.
    static void
    expectNoMetaKey(dom::Value const& value)
    {
        BOOST_TEST(value.isObject());
        if (!value.isObject())
        {
            return;
        }
        dom::Object ctx = value.getObject();
        BOOST_TEST(!ctx.exists("$meta"));
        BOOST_TEST(ctx.get("$meta").isUndefined());
        bool listed = false;
        bool hasCorpus = false;
        ctx.visit([&](dom::String key, dom::Value const&)
            {
                listed = listed || key.get() == "$meta";
                hasCorpus = hasCorpus || key.get() == "corpus";
                return true; });
        BOOST_TEST(!listed);
        BOOST_TEST(hasCorpus);
        BOOST_TEST(ctx.exists("corpus"));
    }

    void
    testNoMetaKey()
    {
        setup("meta.lua", "");
        Expected<Corpus> corpus = Corpus::build(*config_, reporter_);
        BOOST_TEST(corpus.has_value());
        if (!corpus)
        {
            return;
        }
        TransformExtensionContext const transform = makeTransformExtensionContext(
            NamedTransform(), *corpus, *config_, reporter_);
        // The keys added by hand come first, and one that shadows a described
        // field is listed once, with the value added by hand.
        auto const makeExtras = [&]
        {
            dom::Object extras;
            extras.set("report", reportDom(reporter_));
            extras.set("corpus", 1);
            return extras;
        };
        dom::Value const transformCtx(dom::overlayObject(
            makeExtras(),
            dom::newObject<CachedDescribedObject<TransformExtensionContext>>(
                transform)));
        expectNoMetaKey(transformCtx);
        expectExtrasFirst(transformCtx);
        GeneratorExtensionContext const generator = makeGeneratorExtensionContext(
            NamedGenerator(), *corpus, *config_, reporter_);
        dom::Value const generatorCtx(dom::overlayObject(
            makeExtras(),
            dom::newObject<CachedDescribedObject<GeneratorExtensionContext>>(
                generator)));
        expectNoMetaKey(generatorCtx);
        expectExtrasFirst(generatorCtx);
    }

    static void
    expectExtrasFirst(dom::Value const& value)
    {
        BOOST_TEST(value.isObject());
        if (!value.isObject())
        {
            return;
        }
        dom::Object const ctx = value.getObject();
        std::vector<std::string> keys;
        ctx.visit([&](dom::String key, dom::Value const&)
            {
                keys.push_back(std::string(key.get()));
                return true; });
        BOOST_TEST(keys.size() >= 2u);
        if (keys.size() >= 2u)
        {
            BOOST_TEST(keys[0] == "report");
            BOOST_TEST(keys[1] == "corpus");
        }
        BOOST_TEST(std::count(keys.begin(), keys.end(), "corpus") == 1);
        BOOST_TEST(ctx.size() == keys.size());
        BOOST_TEST(ctx.get("corpus").isInteger());
    }

    // A script may keep `ctx` in a global and read it in a later call, after
    // the call that received it has returned.
    void
    testKeptContext()
    {
        for (bool const lua : {true, false})
        {
            setup(lua ? "keep.lua" : "keep.js", lua ? keepLua : keepJs);
            Expected<Corpus> corpus = Corpus::build(*config_, reporter_);
            BOOST_TEST(corpus.has_value());
            if (!corpus)
            {
                return;
            }
            Config config = *config_;
            dom::Object options;
            options.set("tag", "kept");
            config.transformOptions["saver"] = options;
            ExtensionRegistry registry;
            BOOST_TEST(registry.loadExtensions(config, reporter_).has_value());
            BOOST_TEST(registry.applyTransforms(*corpus, config, reporter_).has_value());
            bool renamed = false;
            for (Symbol const& symbol : *corpus)
            {
                renamed = renamed || symbol.Name == "kept";
            }
            BOOST_TEST(renamed);
        }
    }

    void
    run()
    {
        testKeptContext();
        testNoMetaKey();
    }
};

TEST_SUITE(
    CachedDescribedObjectTest,
    "clang.mrdocs.Support.DescribedToDom.CachedDescribedObject");

} // mrdocs
