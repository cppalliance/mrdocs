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
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <mrdocs/Support/Reporter.hpp>
#include <test_suite/test_suite.hpp>
#include "../TestProject.hpp"
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mrdocs {

namespace {

// A script that registers a generator whose id is `@ID@` and whose output is
// `@WHAT@`.
constexpr std::string_view generatorLua = R"lua(mrdocs.register_generator("@ID@", function(ctx)
    ctx.output.write("out.txt", "@WHAT@")
end)
)lua";

// A script that registers a transform that fails when it runs.
constexpr std::string_view failingTransformLua = R"lua(mrdocs.register_transform("boom", function(ctx)
    error("the transform of a refused script ran")
end)
)lua";

// A script that registers a generator of this id, whose output is `what`.
std::string
generatorScript(std::string_view id, std::string_view what)
{
    std::string script(generatorLua);
    for (auto const& [marker, value] : {std::pair{std::string_view("@ID@"), id},
             std::pair{std::string_view("@WHAT@"), what}})
    {
        std::size_t const at = script.find(marker);
        BOOST_TEST(at != std::string::npos);
        if (at != std::string::npos)
        {
            script.replace(at, marker.size(), value);
        }
    }
    return script;
}

// A generator with the given id, as a native extension would install.
class NativeGenerator final
    : public Generator
{
    std::string id_;

public:
    explicit NativeGenerator(std::string_view id)
        : id_(id)
    {
    }

    std::string_view
    id() const noexcept override
    {
        return id_;
    }

    std::string_view
    displayName() const noexcept override
    {
        return "Native";
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

} // (anon)

// The ids that scripts and native extensions register, against the
// generators already in the registry.
struct GeneratorRegistryTest
{
    // The reporter the configurations, corpora and extensions of the test
    // report to, declared first so that it outlives them.
    report::Reporter reporter_;

    // Load the scripts `scripts` (file name and text) into a registry, then,
    // when `nativeId` is not empty, install a native generator of that id.
    // Return the text of the first error. Without one, run the generator
    // `id` into a fresh output directory and return what it wrote to
    // `out.txt`.
    std::string
    runAfterLoading(
        std::vector<std::pair<std::string, std::string>> const& scripts,
        std::string_view id,
        std::string_view nativeId = {})
    {
        TestProject project =
            makeTestProject("mrdocs-genreg", scripts, reporter_);
        Config& config = project.config;

        Expected<Corpus> corpus = Corpus::build(config, reporter_);
        BOOST_TEST(corpus.has_value());
        if (!corpus)
        {
            return "no corpus";
        }
        ExtensionRegistry registry;
        Expected<void> const loaded = registry.loadExtensions(config, reporter_);
        if (!loaded)
        {
            // A refused script leaves nothing behind: loading again is
            // refused the same way, and no transform of it runs.
            Expected<void> const again = registry.loadExtensions(config, reporter_);
            BOOST_TEST(!again);
            if (!again)
            {
                BOOST_TEST(again.error().reason() == loaded.error().reason());
            }
            BOOST_TEST(registry.applyTransforms(*corpus, config, reporter_).has_value());
            return loaded.error().reason();
        }
        if (!nativeId.empty())
        {
            Expected<void> const added = registry.generators().install(
                std::make_shared<NativeGenerator>(nativeId));
            return added ? "installed" : added.error().reason();
        }
        Generator const* const generator = registry.generators().find(id);
        BOOST_TEST(generator != nullptr);
        if (!generator)
        {
            return "no generator";
        }
        Expected<void> const built = generator->build(
            *corpus, config, reporter_);
        if (!built)
        {
            return built.error().reason();
        }
        auto text = files::getFileText(
            files::appendPath(project.out.path(), "out.txt"));
        BOOST_TEST(text.has_value());
        return text.value_or("");
    }

    // Of two scripts that register one id, the one that loads second is
    // refused: the files load in the order of their names.
    void
    testSecondScriptIsRefused()
    {
        std::string const reason = runAfterLoading({{"a.lua", generatorScript("dup", "first")},
                                                       {"b.lua", generatorScript("dup", "second")}},
            "dup");
        BOOST_TEST(reason.find("b.lua") != std::string::npos);
        BOOST_TEST(reason.find("generator id=\"dup\" already exists") !=
            std::string::npos);
    }

    // The transform of a refused script does not run: the script fails
    // when it runs, and the registry stays usable.
    void
    testRefusedScriptRunsNoTransform()
    {
        std::string const reason = runAfterLoading({{"a.lua",
                                                       std::string(failingTransformLua) +
                                                           generatorScript("xml", "from the script")}},
            "xml");
        BOOST_TEST(reason.find("generator id=\"xml\" already exists") !=
            std::string::npos);
    }

    // One script that registers the same id twice is refused too.
    void
    testScriptRegisteringTwiceIsRefused()
    {
        std::string const reason = runAfterLoading({{"a.lua",
                                                       generatorScript("dup", "first") +
                                                           generatorScript("dup", "second")}},
            "dup");
        BOOST_TEST(reason.find("generator id=\"dup\" already exists") !=
            std::string::npos);
    }

    // A script does not take the place of a generator that ships with
    // MrDocs.
    void
    testScriptCannotReplaceBuiltIn()
    {
        std::string const reason = runAfterLoading({{"a.lua", generatorScript("xml", "from the script")}}, "xml");
        BOOST_TEST(reason.find("a.lua") != std::string::npos);
        BOOST_TEST(reason.find("generator id=\"xml\" already exists") !=
            std::string::npos);
    }

    // A native extension that comes after a script is refused when it
    // installs the script's id.
    void
    testNativeAfterScriptIsRefused()
    {
        BOOST_TEST(runAfterLoading({{"a.lua", generatorScript("dup", "first")}}, "dup", "dup") ==
            "generator id=\"dup\" already exists");
    }

    // The ids do not collide, so every script's generator is there.
    void
    testDistinctIdsLoad()
    {
        BOOST_TEST(runAfterLoading({{"a.lua", generatorScript("one", "first")},
                                       {"b.lua", generatorScript("two", "second")}},
                       "two") ==
            "second");
    }

    void
    run()
    {
        testSecondScriptIsRefused();
        testRefusedScriptRunsNoTransform();
        testScriptRegisteringTwiceIsRefused();
        testScriptCannotReplaceBuiltIn();
        testNativeAfterScriptIsRefused();
        testDistinctIdsLoad();
    }
};

TEST_SUITE(
    GeneratorRegistryTest,
    "clang.mrdocs.Extensions.GeneratorRegistry");

} // mrdocs
