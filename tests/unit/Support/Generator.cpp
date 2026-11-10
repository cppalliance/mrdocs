//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Extensions/GeneratorRegistry.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Session.hpp>
#include <test_suite/test_suite.hpp>
#include <memory>
#include <type_traits>


namespace mrdocs {

namespace {

// Mock generator for testing GeneratorRegistry::install
class MockGenerator : public Generator
{
    std::string id_;
    std::string displayName_;
    std::string fileExtension_;

public:
    MockGenerator(
        std::string id,
        std::string displayName,
        std::string fileExtension)
        : id_(std::move(id))
        , displayName_(std::move(displayName))
        , fileExtension_(std::move(fileExtension))
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
        return displayName_;
    }

    std::string_view
    fileExtension() const noexcept override
    {
        return fileExtension_;
    }

    Expected<void>
    build(Corpus const&, Config const&, report::Reporter&) const override
    {
        return {};
    }
};

} // anonymous namespace

static_assert(!std::is_copy_constructible_v<GeneratorRegistry>);
static_assert(!std::is_move_constructible_v<GeneratorRegistry>);
static_assert(!std::is_move_assignable_v<GeneratorRegistry>);

struct GeneratorTest
{
    void
    testFindGenerator()
    {
        GeneratorRegistry registry;

        // Built-in generators are there from the start
        BOOST_TEST(registry.find("adoc") != nullptr);
        BOOST_TEST(registry.find("xml") != nullptr);
        BOOST_TEST(registry.find("html") != nullptr);
        BOOST_TEST(registry.find("json") != nullptr);
        BOOST_TEST(registry.find("noop") != nullptr);
        BOOST_TEST(registry.generators().size() == 5);

        // Non-existent generator should return nullptr
        BOOST_TEST(registry.find("nonexistent") == nullptr);
        BOOST_TEST(registry.find("") == nullptr);
    }

    void
    testAddGenerator()
    {
        GeneratorRegistry registry;
        auto mockGen = std::make_unique<MockGenerator>(
            "mock-test", "Mock Test Generator", "mock");

        auto result = registry.install(std::move(mockGen));
        BOOST_TEST(result.has_value());

        // Should now be findable
        auto* found = registry.find("mock-test");
        BOOST_TEST(found != nullptr);
        if (found)
        {
            BOOST_TEST(found->id() == "mock-test");
            BOOST_TEST(found->displayName() == "Mock Test Generator");
            BOOST_TEST(found->fileExtension() == "mock");
        }
    }

    void
    testAddDuplicateGenerator()
    {
        GeneratorRegistry registry;

        // A generator with the id of a built-in one is refused
        auto result = registry.install(std::make_unique<MockGenerator>(
            "xml", "Duplicate XML", "xml"));
        BOOST_TEST(!result.has_value());
        if (!result)
        {
            BOOST_TEST(
                result.error().reason() == "generator id=\"xml\" already exists");
        }

        // and so is one with the id of a generator added earlier
        BOOST_TEST(registry.install(std::make_unique<MockGenerator>(
                                                 "mock-test", "Mock", "mock"))
                .has_value());
        BOOST_TEST(!registry.install(std::make_unique<MockGenerator>(
                                                  "mock-test", "Mock again", "mock"))
                .has_value());

        // The built-in generator is still the one found
        BOOST_TEST(registry.find("xml")->displayName() != "Duplicate XML");
    }

    void
    testAddNullGenerator()
    {
        GeneratorRegistry registry;
        auto result = registry.install(std::unique_ptr<Generator>{});
        BOOST_TEST(!result.has_value());
    }

    // The same generator object added twice is held once, which is what
    // lets a library's generators reach a registry that loads it again.
    void
    testAddSameGeneratorTwice()
    {
        GeneratorRegistry registry;
        std::shared_ptr<Generator const> generator =
            std::make_shared<MockGenerator>("mock-test", "Mock", "mock");
        BOOST_TEST(registry.install(generator).has_value());
        BOOST_TEST(registry.install(generator).has_value());
        BOOST_TEST(registry.generators().size() == 6);
    }

    // Registries do not see each other's generators.
    void
    testRegistriesAreIndependent()
    {
        GeneratorRegistry first;
        GeneratorRegistry second;
        BOOST_TEST(first.install(std::make_unique<MockGenerator>(
                                              "mock-test", "Mock", "mock"))
                .has_value());
        BOOST_TEST(first.find("mock-test") != nullptr);
        BOOST_TEST(second.find("mock-test") == nullptr);
        BOOST_TEST(second.install(std::make_unique<MockGenerator>(
                                               "mock-test", "Mock", "mock"))
                .has_value());
    }

    // A session owns one generator registry, which its extension registry
    // adds to, and two sessions do not see each other's generators.
    void
    testSession()
    {
        Config config;
        Expected<Session> loadedFirst = Session::create(config);
        Expected<Session> loadedSecond = Session::create(config);
        BOOST_TEST(loadedFirst.has_value());
        BOOST_TEST(loadedSecond.has_value());
        if (!loadedFirst || !loadedSecond)
        {
            return;
        }
        Session& first = *loadedFirst;
        Session& second = *loadedSecond;
        BOOST_TEST(first.generators().find("xml") != nullptr);
        BOOST_TEST(&first.generators() == &first.extensions().generators());
        BOOST_TEST(first.generators().install(
                                         std::make_unique<MockGenerator>(
                                             "mock-test", "Mock", "mock"))
                .has_value());
        BOOST_TEST(first.generators().find("mock-test") != nullptr);
        BOOST_TEST(second.generators().find("mock-test") == nullptr);
    }

    // The block a generator receives is the `generator-options.<id>` block
    // of its own id, and an empty object when the configuration has none.
    void
    testParamsBlock()
    {
        MockGenerator const mock("mock-test", "Mock", "mock");
        Config config;
        BOOST_TEST(getGeneratorParams(mock, config).empty());

        dom::Object other;
        other.set("key", "other");
        config.generatorOptions["xml"] = other;
        BOOST_TEST(getGeneratorParams(mock, config).empty());

        dom::Object own;
        own.set("key", "own");
        config.generatorOptions["mock-test"] = own;
        dom::Object const params = getGeneratorParams(mock, config);
        BOOST_TEST(params.exists("key"));
        BOOST_TEST(params.get("key").getString() == "own");
    }

    void
    run()
    {
        testParamsBlock();
        testFindGenerator();
        testAddGenerator();
        testAddDuplicateGenerator();
        testAddNullGenerator();
        testAddSameGeneratorTwice();
        testRegistriesAreIndependent();
        testSession();
    }
};

TEST_SUITE(
    GeneratorTest,
    "clang.mrdocs.Generator");

} // mrdocs
