//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Plugin.hpp>
#include <mrdocs/Config.hpp>
#include <mrdocs/Config/ReferenceDirectories.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <mrdocs/Support/Filesystem/Temp.hpp>
#include <mrdocs/Transform.hpp>
#include <test_suite/test_suite.hpp>
#include <fstream>
#include <ios>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mrdocs {

namespace {

// Write `content` verbatim to `path`. Pre-existing files are truncated.
void
writeFile(std::string_view path, std::string_view content)
{
    std::ofstream os(std::string{path}, std::ios::binary | std::ios::trunc);
    os.write(content.data(),
             static_cast<std::streamsize>(content.size()));
}

// A transform that appends its id to `log` when it runs, and fails when
// `fails` says so.
class LoggingTransform final
    : public Transform
{
    std::string id_;
    std::vector<std::string>& log_;
    bool fails_;

public:
    LoggingTransform(
        std::string id,
        std::vector<std::string>& log,
        bool fails) noexcept
        : id_(std::move(id))
        , log_(log)
        , fails_(fails)
    {
    }

    std::string_view
    id() const noexcept override
    {
        return id_;
    }

    Expected<void>
    apply(Corpus&, Config const&) const override
    {
        log_.push_back(id_);
        Expected<void> result;
        if (fails_)
        {
            result = Unexpected(Error("deliberate failure"));
        }
        return result;
    }
};

// Return a registry of transforms named first, second and third, in that
// order, logging to `log`; the one named `failing`, if any, fails.
PluginRegistry
makeRegistry(std::vector<std::string>& log, std::string_view failing)
{
    std::vector<std::unique_ptr<Transform>> transforms;
    for (std::string_view id : { "first", "second", "third" })
    {
        transforms.push_back(std::make_unique<LoggingTransform>(
            std::string(id), log, id == failing));
    }
    return PluginRegistry(std::move(transforms));
}

} // (anon)

// `PluginRegistry::applyTransforms`, which runs the transforms plugins
// installed. They run over a small real corpus built once for the suite;
// its source pulls in no headers, so extraction needs no toolchain
// resource directory.
struct PluginRegistryTest
{
    std::optional<Config> config_;
    std::optional<Corpus> corpus_;

    void
    setup()
    {
        ScopedTempDirectory src("mrdocs-pluginregistry-src");
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

    void
    testInstallOrder()
    {
        std::vector<std::string> log;
        PluginRegistry const registry = makeRegistry(log, "");
        BOOST_TEST(registry.applyTransforms(*corpus_, *config_).has_value());
        BOOST_TEST((log == std::vector<std::string>{ "first", "second", "third" }));
    }

    void
    testStopsAtFirstError()
    {
        std::vector<std::string> log;
        PluginRegistry const registry = makeRegistry(log, "second");
        BOOST_TEST(!registry.applyTransforms(*corpus_, *config_).has_value());
        BOOST_TEST((log == std::vector<std::string>{ "first", "second" }));
    }

    void
    testErrorNamesTransform()
    {
        std::vector<std::string> log;
        PluginRegistry const registry = makeRegistry(log, "second");
        Expected<void> const result =
            registry.applyTransforms(*corpus_, *config_);
        BOOST_TEST(!result.has_value());
        if (!result)
        {
            BOOST_TEST(result.error().reason() ==
                "the transform \"second\" failed: deliberate failure");
        }
    }

    void
    run()
    {
        setup();
        if (corpus_)
        {
            testInstallOrder();
            testStopsAtFirstError();
            testErrorNamesTransform();
        }
    }
};

TEST_SUITE(
    PluginRegistryTest,
    "clang.mrdocs.PluginRegistry");

} // mrdocs
