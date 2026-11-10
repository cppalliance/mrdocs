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
#include <mrdocs/Config/ReferenceDirectories.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <mrdocs/Support/Filesystem/Temp.hpp>
#include <mrdocs/Support/Reporter.hpp>
#include <test_suite/test_suite.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <cstdlib>

namespace mrdocs {

struct ConfigTest
{
    void
    testDeprecatedOptionFromFile()
    {
        // A legacy configuration key forwards its value to the replacement
        // option, so a rename never silently drops a user's setting.
        Config c;
        auto const result = Config::load(
            c, "extract-implicit-specializations: false");
        BOOST_TEST(result.has_value());
        BOOST_TEST(!c.extractImplicitBaseClasses);
    }

    void
    testDeprecatedOptionFromCommandLine()
    {
        char const* argv[] = {
            "mrdocs", "--extract-implicit-specializations=false", nullptr};
        Config c;
        auto const result = Config::load(c, "", argv);
        BOOST_TEST(result.has_value());
        BOOST_TEST(!c.extractImplicitBaseClasses);
    }

    // An unknown key is a warning of the core, so `warn-as-error` makes
    // it an error and the run fails, and without it the run passes.
    void
    testUnknownKeyUnderWarnAsError()
    {
        for (bool const warnAsError : {false, true})
        {
            Config c;
            auto const result = Config::load(c, "no-such-option: 1");
            BOOST_TEST(result.has_value());
            report::Reporter reporter;
            reporter.setMinimumLevel(report::Level::fatal);
            reporter.setWarnAsError(warnAsError);
            c.reportUnknownConfigKeys(reporter);
            report::Results const results = reporter.results();
            BOOST_TEST(results.warnCount == (warnAsError ? 0u : 1u));
            BOOST_TEST(results.errorCount == (warnAsError ? 1u : 0u));
        }
    }

    // A warning that normalizing the options reports, here a system include
    // directory that does not exist, is an error under `warn-as-error`.
    void
    testMissingDirectoryUnderWarnAsError()
    {
        for (bool const warnAsError : {false, true})
        {
            ScopedTempDirectory dir("mrdocs-config-test");
            BOOST_TEST(dir);
            std::string const root(dir.path());
            std::string const configPath = files::appendPath(root, "mrdocs.yml");
            std::string const missing = files::appendPath(root, "no-such-dir");
            BOOST_TEST(files::writeFileText(configPath,
                "source-root: " + root + "\naddons: " + root + "\ninput:\n  - " +
                    root + "\nwarn-as-error: " +
                    (warnAsError ? "true" : "false") +
                    "\nsystem-includes:\n  - " + missing + "\n")
                    .has_value());
            ReferenceDirectories dirs;
            dirs.cwd = root;
            dirs.mrdocsRoot = root;
            Config c;
            report::Reporter reporter;
            auto const loaded = Config::loadFile(c, configPath, dirs, reporter);
            BOOST_TEST(loaded.has_value());
            report::Results const results = reporter.results();
            // The default header directories do not exist in the temporary
            // tree either, so more than one warning is reported
            if (warnAsError)
            {
                BOOST_TEST(results.warnCount == 0u);
                BOOST_TEST(results.errorCount >= 1u);
            }
            else
            {
                BOOST_TEST(results.warnCount >= 1u);
                BOOST_TEST(results.errorCount == 0u);
            }
        }
    }

    // A configuration loaded from a string stands for the file at the given
    // path, which need not exist: relative paths and `configDir()` resolve
    // against that path's directory, and an unknown key adds exactly one
    // warning. Loading a missing file still fails.
    void
    testLoadString()
    {
        ScopedTempDirectory dir("mrdocs-config-string-test");
        BOOST_TEST(dir);
        std::string const root(dir.path());
        std::string const sub = files::appendPath(root, "sub");
        std::string const inner = files::appendPath(sub, "inner");
        BOOST_TEST(files::createDirectory(sub).has_value());
        BOOST_TEST(files::createDirectory(inner).has_value());
        std::string const configPath = files::appendPath(sub, "mrdocs.yml");
        ReferenceDirectories dirs;
        dirs.cwd = root;
        dirs.mrdocsRoot = root;
        std::string const base =
            "source-root: inner\naddons: inner\ninput:\n  - inner\n"
            "output: docs\n";
        unsigned warnings[2] = {};
        for (int const withUnknown : {0, 1})
        {
            Config c;
            report::Reporter reporter;
            auto const loaded = Config::loadString(c,
                withUnknown ? base + "no-such-option: 1\n" : base,
                configPath, dirs, reporter);
            BOOST_TEST(loaded.has_value());
            warnings[withUnknown] = reporter.results().warnCount;
            if (withUnknown)
            {
                continue;
            }
            BOOST_TEST(files::makePosixStyle(c.configDir()) ==
                files::makePosixStyle(sub));
            BOOST_TEST(files::makePosixStyle(c.config) ==
                files::makePosixStyle(configPath));
            BOOST_TEST(files::makePosixStyle(c.sourceRoot) ==
                files::makePosixStyle(inner));
            BOOST_TEST(files::makePosixStyle(c.addons) ==
                files::makePosixStyle(inner));
            BOOST_TEST(files::makePosixStyle(c.output) ==
                files::makePosixStyle(files::appendPath(sub, "docs")));
            BOOST_TEST(c.input.size() == 1u);
            BOOST_TEST(files::makePosixStyle(c.input.front()) ==
                files::makePosixStyle(inner));
        }
        BOOST_TEST(warnings[1] == warnings[0] + 1u);

        // The exemption covers the `config` option alone
        {
            Config c;
            report::Reporter reporter;
            auto const loaded = Config::loadString(c,
                "source-root: missing-dir\n", configPath, dirs, reporter);
            BOOST_TEST(!loaded.has_value());
        }

        // Without a string, the `config` option must name an existing file
        {
            ReferenceDirectories emptyDirs;
            emptyDirs.cwd = inner;
            emptyDirs.mrdocsRoot = inner;
            Config c;
            report::Reporter reporter;
            BOOST_TEST(Config::load(c, "").has_value());
            auto const normalized = c.normalize(emptyDirs, reporter);
            BOOST_TEST(!normalized.has_value());
            if (!normalized)
            {
                BOOST_TEST(
                    normalized.error().message().find(
                        "`config` option: path does not exist") !=
                    std::string::npos);
            }
        }

        // Only a string stands for a file that need not exist: loading a
        // missing file fails and names it.
        std::string const missing = files::appendPath(sub, "missing.yml");
        Config c;
        report::Reporter reporter;
        auto const loaded = Config::loadFile(c, missing, dirs, reporter);
        BOOST_TEST(!loaded.has_value());
        if (!loaded)
        {
            BOOST_TEST(
                loaded.error().message().find(missing) !=
                std::string::npos);
        }
    }

    // The root derives from the file that holds the core. In these tests that
    // is the test executable, which ctest starts as `./mrdocs-unit-tests`
    // from its own directory, so the loader reports it by a relative path.
    // The root is absolute and two levels below the executable all the same.
    void
    testRootFromRelativelyStartedExecutable()
    {
        if (char const* env = std::getenv("MRDOCS_ROOT"); env && *env)
        {
            return;
        }
        static int anchor;
        std::string const exe = llvm::sys::fs::getMainExecutable(
            nullptr, &anchor);
        BOOST_TEST(llvm::sys::path::is_absolute(exe));
        ReferenceDirectories const dirs;
        BOOST_TEST(llvm::sys::path::is_absolute(dirs.mrdocsRoot));
        BOOST_TEST(dirs.mrdocsRoot == files::getParentDir(exe, 2));
    }

    void
    run()
    {
        testRootFromRelativelyStartedExecutable();
        testDeprecatedOptionFromFile();
        testDeprecatedOptionFromCommandLine();
        testUnknownKeyUnderWarnAsError();
        testMissingDirectoryUnderWarnAsError();
        testLoadString();
    }
};

TEST_SUITE(
    ConfigTest,
    "clang.mrdocs.Config");

} // mrdocs
