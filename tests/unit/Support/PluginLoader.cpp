//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Support/PluginLoader.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <mrdocs/Support/Filesystem/Temp.hpp>
#include <test_suite/test_suite.hpp>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace mrdocs {

namespace {

// The extension a plugin carries on this platform.
#ifdef _WIN32
constexpr std::string_view libraryExtension = ".dll";
#elif defined(__APPLE__)
constexpr std::string_view libraryExtension = ".dylib";
#else
constexpr std::string_view libraryExtension = ".so";
#endif

// Return the path of a plugin library named `stem` in `dir`.
std::string
libraryPath(
    std::string_view dir,
    std::string_view stem)
{
    return files::appendPath(
        dir, std::string(stem) + std::string(libraryExtension));
}

// Create an empty file at `path`. Discovery reports a file by name and
// never opens it, so the contents do not matter here.
void
writeFile(std::string_view path)
{
    std::ofstream os(std::string{path}, std::ios::binary | std::ios::trunc);
}

// Create a directory, and the directories leading to it.
void
makeDirectory(std::string_view path)
{
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
}

// Create `<root>/plugins` and return its path.
std::string
makePluginDir(std::string_view root)
{
    std::string const dir = files::appendPath(root, "plugins");
    makeDirectory(dir);
    return dir;
}

} // (anon)

struct PluginLoaderTest
{
    void
    testRootWithoutPluginDir()
    {
        ScopedTempDirectory td("mrdocs-plugins");
        BOOST_TEST(td);
        std::string const root(td.path());

        BOOST_TEST(discoverPlugins({ root }).empty());
    }

    void
    testMissingRoot()
    {
        ScopedTempDirectory td("mrdocs-plugins");
        BOOST_TEST(td);
        std::string const root = files::appendPath(td.path(), "absent");

        BOOST_TEST(discoverPlugins({ root }).empty());
    }

    void
    testOnlyLibraries()
    {
        ScopedTempDirectory td("mrdocs-plugins");
        BOOST_TEST(td);
        std::string const root(td.path());
        std::string const dir = makePluginDir(root);
        writeFile(libraryPath(dir, "stats"));
        // Anything that is not a library is left alone: the directory
        // MrDocs ships documents itself with a README, and a build can
        // leave an import library or debug information behind.
        writeFile(files::appendPath(dir, "README.adoc"));
        writeFile(files::appendPath(dir, "stats.lib"));
        makeDirectory(libraryPath(dir, "nested"));

        std::vector<std::string> const found = discoverPlugins({ root });
        BOOST_TEST(found.size() == 1);
        if (!found.empty())
        {
            BOOST_TEST(files::getFileName(found.front()).starts_with("stats."));
        }
    }

    void
    testNameOrderWithinRoot()
    {
        ScopedTempDirectory td("mrdocs-plugins");
        BOOST_TEST(td);
        std::string const root(td.path());
        std::string const dir = makePluginDir(root);
        // Created out of order: the load order comes from the names, not
        // from the order the filesystem reports entries in.
        writeFile(libraryPath(dir, "second"));
        writeFile(libraryPath(dir, "first"));

        std::vector<std::string> const found = discoverPlugins({ root });
        BOOST_TEST(found.size() == 2);
        if (found.size() == 2)
        {
            BOOST_TEST(files::getFileName(found[0]).starts_with("first."));
            BOOST_TEST(files::getFileName(found[1]).starts_with("second."));
        }
    }

    void
    testRootOrder()
    {
        ScopedTempDirectory td("mrdocs-plugins");
        BOOST_TEST(td);
        std::string const primary = files::appendPath(td.path(), "primary");
        std::string const supplemental =
            files::appendPath(td.path(), "supplemental");
        // The library in the supplemental root sorts first by name, so a
        // result in root order can only come from the roots themselves
        // being searched in the order they were given.
        writeFile(libraryPath(makePluginDir(primary), "zzz"));
        writeFile(libraryPath(makePluginDir(supplemental), "aaa"));

        std::vector<std::string> const found =
            discoverPlugins({ primary, supplemental });
        BOOST_TEST(found.size() == 2);
        if (found.size() == 2)
        {
            BOOST_TEST(files::getFileName(found[0]).starts_with("zzz."));
            BOOST_TEST(files::getFileName(found[1]).starts_with("aaa."));
        }
    }

    void
    testRepeatedRoot()
    {
        ScopedTempDirectory td("mrdocs-plugins");
        BOOST_TEST(td);
        std::string const root(td.path());
        writeFile(libraryPath(makePluginDir(root), "stats"));
        // A configuration can name one root twice, directly or through a
        // link. Loading the library again would run its entry point a
        // second time and fail on the id it already installed.
        std::vector<std::string> const found =
            discoverPlugins({ root, root });
        BOOST_TEST(found.size() == 1);
    }

    // CMake gives a module library the .so extension on macOS, so both
    // spellings have to be found there.
    void
    testAppleExtensions()
    {
#ifdef __APPLE__
        ScopedTempDirectory td("mrdocs-plugins");
        BOOST_TEST(td);
        std::string const root(td.path());
        std::string const dir = makePluginDir(root);
        writeFile(files::appendPath(dir, "bundle.so"));
        writeFile(files::appendPath(dir, "library.dylib"));

        BOOST_TEST(discoverPlugins({ root }).size() == 2);
#endif
    }

    void
    run()
    {
        testRootWithoutPluginDir();
        testMissingRoot();
        testOnlyLibraries();
        testNameOrderWithinRoot();
        testRootOrder();
        testRepeatedRoot();
        testAppleExtensions();
    }
};

TEST_SUITE(
    PluginLoaderTest,
    "clang.mrdocs.PluginLoader");

} // mrdocs
