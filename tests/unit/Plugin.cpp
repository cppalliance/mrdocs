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
#include <mrdocs/Extensions/ExtensionRegistry.hpp>
#include <mrdocs/Extensions/GeneratorExtensionContext.hpp>
#include <mrdocs/Extensions/TransformExtensionContext.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Plugin.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <mrdocs/Support/Reporter.hpp>
#include <mrdocs/Transform.hpp>
#include <test_suite/test_suite.hpp>
#include "TestProject.hpp"
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace mrdocs {

namespace {

// The context a plugin entry point receives, backed by a registry the way
// the plugin loader backs it.
class TestPluginContext final
    : public PluginContext
{
    Config const& config_;
    ExtensionRegistry& registry_;

public:
    TestPluginContext(Config const& config, ExtensionRegistry& registry)
        : config_(config)
        , registry_(registry)
    {
    }

    Config const&
    config() const noexcept override
    {
        return config_;
    }

    Expected<void>
    registerGenerator(std::unique_ptr<Generator> G) override
    {
        return registry_.generators().install(
            std::shared_ptr<Generator const>(std::move(G)));
    }

    Expected<void>
    registerTransform(std::unique_ptr<Transform> T) override
    {
        return registry_.installTransform(
            std::shared_ptr<Transform const>(std::move(T)));
    }
};

} // (anon)

// The overloads of PluginContext that take an id and a callable.
struct PluginTest
{
    // The reporter the configurations, corpora and extensions of the test
    // report to, declared first so that it outlives them.
    report::Reporter reporter_;

    std::optional<TestProject> project_;
    Config const* config_ = nullptr;

    void
    setup()
    {
        project_.emplace(makeTestProject("mrdocs-callable", {}, reporter_));
        config_ = &project_->config;
    }

    // The callable runs with the corpus and its own params block, in the
    // pipeline, and an empty object stands in for a missing block.
    void
    testTransformRuns()
    {
        Expected<Corpus> corpus = Corpus::build(*config_, reporter_);
        BOOST_TEST(corpus.has_value());
        if (!corpus)
        {
            return;
        }
        Config config = *config_;
        dom::Object options;
        options.set("suffix", "-callable");
        config.transformOptions["suffixer"] = options;

        ExtensionRegistry registry;
        TestPluginContext context(config, registry);
        PluginContext& plugin = context;
        std::string withoutBlock = "unset";
        BOOST_TEST(plugin.registerTransform("suffixer",
            [](TransformExtensionContext const& ctx) -> Expected<void>
            {
                for (Symbol const& symbol : *ctx.corpus)
                {
                    if (symbol.Name == "Widget")
                    {
                        ctx.corpus->find(symbol.id)->Name +=
                            std::string(ctx.params.get("suffix").getString());
                    }
                }
                return {};
            }).has_value());
        BOOST_TEST(plugin.registerTransform("lonely",
            [&withoutBlock](TransformExtensionContext const& ctx) -> Expected<void>
            {
                withoutBlock = ctx.params.empty() ? "empty" : "not empty";
                return {};
            }).has_value());
        BOOST_TEST(registry.applyTransforms(*corpus, config, reporter_).has_value());
        bool renamed = false;
        for (Symbol const& symbol : *corpus)
        {
            renamed = renamed || symbol.Name == "Widget-callable";
        }
        BOOST_TEST(renamed);
        BOOST_TEST(withoutBlock == "empty");
    }

    // An error from the callable stops the run, and the message names the
    // transform's id.
    void
    testTransformErrorStopsTheRun()
    {
        Expected<Corpus> corpus = Corpus::build(*config_, reporter_);
        BOOST_TEST(corpus.has_value());
        if (!corpus)
        {
            return;
        }
        ExtensionRegistry registry;
        TestPluginContext context(*config_, registry);
        PluginContext& plugin = context;
        bool laterRan = false;
        BOOST_TEST(plugin.registerTransform("boom",
            [](TransformExtensionContext const&) -> Expected<void>
            {
                return Unexpected(Error("deliberate failure"));
            }).has_value());
        BOOST_TEST(plugin.registerTransform("later",
            [&laterRan](TransformExtensionContext const&) -> Expected<void>
            {
                laterRan = true;
                return {};
            }).has_value());
        Expected<void> const result =
            registry.applyTransforms(*corpus, *config_, reporter_);
        BOOST_TEST(!result.has_value());
        BOOST_TEST(!laterRan);
        if (!result)
        {
            BOOST_TEST(result.error().reason() ==
                "the transform \"boom\" failed: deliberate failure");
        }
    }

    // The generator is selectable under its id, with the display name and
    // extension given; it writes where the context says and reads its own
    // options block.
    void
    testGeneratorRuns()
    {
        Expected<Corpus> corpus = Corpus::build(*config_, reporter_);
        BOOST_TEST(corpus.has_value());
        if (!corpus)
        {
            return;
        }
        Config config = *config_;
        dom::Object options;
        options.set("greeting", "hello");
        config.generatorOptions["notes"] = options;

        ExtensionRegistry registry;
        TestPluginContext context(config, registry);
        PluginContext& plugin = context;
        auto const writeNotes = [](GeneratorExtensionContext const& ctx) -> Expected<void>
        {
            std::string const text =
                std::string(ctx.params.get("greeting").getString()) +
                ":" + std::to_string(ctx.corpus->size() > 0);
            return files::writeFileText(
                files::appendPath(ctx.outputPath, "out.txt"), text);
        };
        BOOST_TEST(plugin.registerGenerator("notes", writeNotes, "Notes", "txt").has_value());
        BOOST_TEST(plugin.registerGenerator("plain",
            [](GeneratorExtensionContext const& ctx) -> Expected<void>
            {
                return files::writeFileText(
                    files::appendPath(ctx.outputPath, "plain.txt"),
                    ctx.params.empty() ? "empty" : "not empty");
            }).has_value());

        Generator const* const notes = registry.generators().find("notes");
        BOOST_TEST(notes != nullptr);
        if (!notes)
        {
            return;
        }
        BOOST_TEST(notes->id() == "notes");
        BOOST_TEST(notes->displayName() == "Notes");
        BOOST_TEST(notes->fileExtension() == "txt");
        BOOST_TEST(notes->build(
                            *corpus, config, reporter_)
                .has_value());
        auto const notesText = files::getFileText(
            files::appendPath(project_->out.path(), "out.txt"));
        BOOST_TEST(notesText.has_value());
        BOOST_TEST(notesText.value_or("") == "hello:1");

        Generator const* const plain = registry.generators().find("plain");
        BOOST_TEST(plain != nullptr);
        if (!plain)
        {
            return;
        }
        BOOST_TEST(plain->displayName() == "plain");
        BOOST_TEST(plain->fileExtension() == "");
        BOOST_TEST(plain->build(
                            *corpus, config, reporter_)
                .has_value());
        auto const plainText = files::getFileText(
            files::appendPath(project_->out.path(), "plain.txt"));
        BOOST_TEST(plainText.has_value());
        BOOST_TEST(plainText.value_or("") == "empty");
    }

    // An error from the callable is the error the build returns; the caller
    // names the generator.
    void
    testGeneratorErrorIsReturned()
    {
        Expected<Corpus> corpus = Corpus::build(*config_, reporter_);
        BOOST_TEST(corpus.has_value());
        if (!corpus)
        {
            return;
        }
        ExtensionRegistry registry;
        TestPluginContext context(*config_, registry);
        PluginContext& plugin = context;
        BOOST_TEST(plugin.registerGenerator("boom",
            [](GeneratorExtensionContext const&) -> Expected<void>
            {
                return Unexpected(Error("deliberate failure"));
            }).has_value());
        Generator const* const generator =
            registry.generators().find("boom");
        BOOST_TEST(generator != nullptr);
        if (!generator)
        {
            return;
        }
        Expected<void> const result = generator->build(
            *corpus, *config_, reporter_);
        BOOST_TEST(!result.has_value());
        if (!result)
        {
            BOOST_TEST(result.error().reason() ==
                "deliberate failure");
        }
    }

    // A generator id that is taken, by a generator that ships with MrDocs or
    // by an earlier registration, is refused.
    void
    testDuplicateGeneratorIsRefused()
    {
        ExtensionRegistry registry;
        TestPluginContext context(*config_, registry);
        PluginContext& plugin = context;
        auto noop = [](GeneratorExtensionContext const&) -> Expected<void>
        {
            return {};
        };
        BOOST_TEST(plugin.registerGenerator("once", noop).has_value());
        Expected<void> const again = plugin.registerGenerator("once", noop);
        BOOST_TEST(!again.has_value());
        if (!again)
        {
            BOOST_TEST(again.error().reason() ==
                "generator id=\"once\" already exists");
        }
        BOOST_TEST(!plugin.registerGenerator("xml", noop).has_value());
    }

    void
    run()
    {
        setup();
        testTransformRuns();
        testTransformErrorStopsTheRun();
        testGeneratorRuns();
        testGeneratorErrorIsReturned();
        testDuplicateGeneratorIsRefused();
    }
};

TEST_SUITE(
    PluginTest,
    "clang.mrdocs.Plugin");

} // mrdocs
