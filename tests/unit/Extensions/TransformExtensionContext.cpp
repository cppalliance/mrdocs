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
#include <mrdocs/Extensions/TransformExtensionContext.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <mrdocs/Support/Reporter.hpp>
#include <mrdocs/Transform.hpp>
#include <test_suite/test_suite.hpp>
#include <string>
#include <string_view>

namespace mrdocs {

namespace {

// A transform that only has an id.
class NamedTransform final
    : public Transform
{
    std::string id_;

public:
    explicit NamedTransform(std::string id)
        : id_(std::move(id))
    {
    }

    std::string_view
    id() const noexcept override
    {
        return id_;
    }

    Expected<void>
    apply(Corpus&, Config const&, report::Reporter&) const override
    {
        return {};
    }
};

} // (anon)

struct TransformExtensionContextTest
{
    // The block a transform receives is the `transform-options.<id>` block
    // of its own id, and an empty object when the configuration has none.
    void
    testParamsBlock()
    {
        NamedTransform const probe("probe");
        Config config;
        BOOST_TEST(getTransformParams(probe, config).empty());

        dom::Object other;
        other.set("key", "other");
        config.transformOptions["elsewhere"] = other;
        BOOST_TEST(getTransformParams(probe, config).empty());

        dom::Object own;
        own.set("key", "own");
        config.transformOptions["probe"] = own;
        dom::Object const params = getTransformParams(probe, config);
        BOOST_TEST(params.exists("key"));
        BOOST_TEST(params.get("key").getString() == "own");
    }

    void
    run()
    {
        testParamsBlock();
    }
};

TEST_SUITE(
    TransformExtensionContextTest,
    "clang.mrdocs.Extensions.TransformExtensionContext");

} // mrdocs
