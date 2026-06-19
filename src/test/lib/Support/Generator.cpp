//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Generator.hpp>
#include <test_suite/test_suite.hpp>
#include <memory>


namespace mrdocs {

namespace {

// Mock generator for testing installGenerator
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
    build(Corpus const&) const override
    {
        return {};
    }
};

} // anonymous namespace

struct GeneratorTest
{
    void
    testFindGenerator()
    {
        // Built-in generators should be found
        BOOST_TEST(findGenerator("adoc") != nullptr);
        BOOST_TEST(findGenerator("xml") != nullptr);
        BOOST_TEST(findGenerator("html") != nullptr);

        // Non-existent generator should return nullptr
        BOOST_TEST(findGenerator("nonexistent") == nullptr);
        BOOST_TEST(findGenerator("") == nullptr);
    }

    void
    testInstallGenerator()
    {
        // Install a new generator
        auto mockGen = std::make_unique<MockGenerator>(
            "mock-test", "Mock Test Generator", "mock");

        auto result = installGenerator(std::move(mockGen));
        BOOST_TEST(result.has_value());

        // Should now be findable
        auto* found = findGenerator("mock-test");
        BOOST_TEST(found != nullptr);
        if (found)
        {
            BOOST_TEST(found->id() == "mock-test");
            BOOST_TEST(found->displayName() == "Mock Test Generator");
            BOOST_TEST(found->fileExtension() == "mock");
        }
    }

    void
    testInstallDuplicateGenerator()
    {
        // Try to install a generator with an existing id
        auto duplicateGen = std::make_unique<MockGenerator>(
            "adoc", "Duplicate Adoc", "adoc");

        auto result = installGenerator(std::move(duplicateGen));
        BOOST_TEST(!result.has_value());
    }

    void
    testInstallNullGenerator()
    {
        // Try to install a null generator
        auto result = installGenerator(std::unique_ptr<Generator>{});
        BOOST_TEST(!result.has_value());
    }

    void
    run()
    {
        testFindGenerator();
        testInstallGenerator();
        testInstallDuplicateGenerator();
        testInstallNullGenerator();
    }
};

TEST_SUITE(
    GeneratorTest,
    "clang.mrdocs.Generator");

} // mrdocs
