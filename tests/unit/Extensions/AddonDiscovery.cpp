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

#include <mrdocs/Extensions/AddonDiscovery.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <mrdocs/Support/Filesystem/Temp.hpp>
#include <mrdocs/Support/Reporter.hpp>
#include <test_suite/test_suite.hpp>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace mrdocs {

namespace {

// The extension a native extension carries on this platform.
#ifdef _WIN32
constexpr std::string_view libraryExtension = ".dll";
#elif defined(__APPLE__)
constexpr std::string_view libraryExtension = ".dylib";
#else
constexpr std::string_view libraryExtension = ".so";
#endif

// Return the name of a library named `stem` on this platform.
std::string
libraryName(std::string_view stem)
{
    return std::string(stem) + std::string(libraryExtension);
}

// Create an empty file at `path` (UTF-8). Discovery reports a file by name
// and never opens it, so the contents do not matter here.
void
writeFile(std::string_view path)
{
    BOOST_TEST(files::writeFileText(path, "").has_value());
}

// Create <root>/extensions and return its path.
std::string
makeExtensionsDir(std::string_view root)
{
    std::string const dir = files::appendPath(root, "extensions");
    BOOST_TEST(files::createDirectory(dir).has_value());
    return dir;
}

// Create the directory <extensions>/<name> and return its path.
std::string
makeExtensionDir(std::string_view extensions, std::string_view name)
{
    std::string const dir = files::appendPath(extensions, name);
    BOOST_TEST(files::createDirectory(dir).has_value());
    return dir;
}

// The names of the entry files of `entries`, to compare a list at a glance.
std::vector<std::string>
fileNames(std::vector<ExtensionEntry> const& entries)
{
    std::vector<std::string> names;
    for (ExtensionEntry const& entry : entries)
    {
        names.push_back(std::string(files::getFileName(entry.path)));
    }
    return names;
}

} // (anon)

struct AddonDiscoveryTest
{
    // Discovery reports the directories it skips and cannot read here.
    report::Reporter reporter_;

    void
    testRootWithoutExtensionsDir()
    {
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const root(td.path());

        Expected<std::vector<ExtensionEntry>> const found =
            collectExtensions({root}, reporter_);
        BOOST_TEST(found.has_value());
        BOOST_TEST((found && found->empty()));
    }

    void
    testMissingRoot()
    {
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const root = files::appendPath(td.path(), "absent");

        Expected<std::vector<ExtensionEntry>> const found =
            collectExtensions({root}, reporter_);
        BOOST_TEST(found.has_value());
        BOOST_TEST((found && found->empty()));
    }

    // A file entry is a script or a native extension by its extension.
    // Anything else in the directory is left alone: the directory MrDocs
    // ships documents itself with a README, and a build can leave an import
    // library or debug information behind.
    void
    testFileEntriesOfAllKinds()
    {
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const root(td.path());
        std::string const dir = makeExtensionsDir(root);
        writeFile(files::appendPath(dir, "a.lua"));
        writeFile(files::appendPath(dir, "b.js"));
        writeFile(files::appendPath(dir, libraryName("c")));
        writeFile(files::appendPath(dir, "README.md"));
        writeFile(files::appendPath(dir, "c.lib"));
        writeFile(files::appendPath(dir, "d.txt"));

        auto const found = collectExtensions({root}, reporter_);
        BOOST_TEST(found.has_value());
        if (!found || found->size() != 3)
        {
            BOOST_TEST((found && found->size() == 3));
            return;
        }
        BOOST_TEST((*found)[0].kind == ExtensionKind::Lua);
        BOOST_TEST((*found)[1].kind == ExtensionKind::JavaScript);
        BOOST_TEST((*found)[2].kind == ExtensionKind::Native);
        for (ExtensionEntry const& entry : *found)
        {
            // A file directly in extensions/ has no directory of its own.
            BOOST_TEST(entry.directory.empty());
            // Paths use forward slashes, like every path MrDocs reports.
            BOOST_TEST(entry.path.find('\\') == std::string::npos);
            BOOST_TEST(files::getParentDir(entry.path) ==
                files::makePosixStyle(dir));
        }
    }

    // A directory is an extension named after it, whose entry file is
    // `<name>.<ext>` or the usual name of a directory module of its language.
    void
    testDirectoryEntries()
    {
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const root(td.path());
        std::string const dir = makeExtensionsDir(root);
        std::string const luaNamed = makeExtensionDir(dir, "a");
        writeFile(files::appendPath(luaNamed, "a.lua"));
        writeFile(files::appendPath(luaNamed, "rules.lua"));
        std::string const luaInit = makeExtensionDir(dir, "b");
        writeFile(files::appendPath(luaInit, "init.lua"));
        writeFile(files::appendPath(luaInit, "rules.lua"));
        std::string const jsNamed = makeExtensionDir(dir, "c");
        writeFile(files::appendPath(jsNamed, "c.js"));
        std::string const jsIndex = makeExtensionDir(dir, "d");
        writeFile(files::appendPath(jsIndex, "index.js"));
        std::string const nativeNamed = makeExtensionDir(dir, "e");
        writeFile(files::appendPath(nativeNamed, libraryName("e")));
        writeFile(files::appendPath(nativeNamed, libraryName("dependency")));
#ifndef _WIN32
        // `lib<name>` is not an entry name on Windows.
        std::string const nativePrefixed = makeExtensionDir(dir, "f");
        writeFile(files::appendPath(nativePrefixed, "lib" + libraryName("f")));
        std::size_t const expected = 6;
#else
        std::size_t const expected = 5;
#endif

        auto const found = collectExtensions({root}, reporter_);
        BOOST_TEST(found.has_value());
        if (!found || found->size() != expected)
        {
            BOOST_TEST((found && found->size() == expected));
            return;
        }
        BOOST_TEST((*found)[0].kind == ExtensionKind::Lua);
        BOOST_TEST((*found)[0].path ==
            files::makePosixStyle(files::appendPath(luaNamed, "a.lua")));
        BOOST_TEST((*found)[0].directory == files::makePosixStyle(luaNamed));
        BOOST_TEST((*found)[1].kind == ExtensionKind::Lua);
        BOOST_TEST(files::getFileName((*found)[1].path) == "init.lua");
        BOOST_TEST((*found)[1].directory == files::makePosixStyle(luaInit));
        BOOST_TEST((*found)[2].kind == ExtensionKind::JavaScript);
        BOOST_TEST(files::getFileName((*found)[2].path) == "c.js");
        BOOST_TEST((*found)[3].kind == ExtensionKind::JavaScript);
        BOOST_TEST(files::getFileName((*found)[3].path) == "index.js");
        BOOST_TEST((*found)[4].kind == ExtensionKind::Native);
        BOOST_TEST(files::getFileName((*found)[4].path) == libraryName("e"));
        BOOST_TEST((*found)[4].directory == files::makePosixStyle(nativeNamed));
#ifndef _WIN32
        BOOST_TEST((*found)[5].kind == ExtensionKind::Native);
        BOOST_TEST(files::getFileName((*found)[5].path) ==
            "lib" + libraryName("f"));
#endif
    }

    // In a directory named `init`, `<name>.lua` and `init.lua` are one
    // file, which is one entry and not two candidates.
    void
    testDirectoryNamedInit()
    {
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const root(td.path());
        std::string const dir = makeExtensionsDir(root);
        std::string const init = makeExtensionDir(dir, "init");
        writeFile(files::appendPath(init, "init.lua"));
        std::string const index = makeExtensionDir(dir, "index");
        writeFile(files::appendPath(index, "index.js"));

        auto const found = collectExtensions({root}, reporter_);
        BOOST_TEST(found.has_value());
        if (found && found->size() == 2)
        {
            BOOST_TEST(files::getFileName((*found)[0].path) == "index.js");
            BOOST_TEST(files::getFileName((*found)[1].path) == "init.lua");
        }
        else
        {
            BOOST_TEST((found && found->size() == 2));
        }
    }

    // On a filesystem that ignores case, `Init/init.lua` has `Init.lua` and
    // `init.lua` as candidates, and they are one file.
    void
    testDirectoryNamedInitDifferingInCase()
    {
#if defined(_WIN32) || defined(__APPLE__)
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const root(td.path());
        std::string const dir = makeExtensionsDir(root);
        std::string const init = makeExtensionDir(dir, "Init");
        writeFile(files::appendPath(init, "init.lua"));

        auto const found = collectExtensions({root}, reporter_);
        BOOST_TEST(found.has_value());
        BOOST_TEST((found && found->size() == 1));
#endif
    }

    // A directory with no entry file is not an extension. It may hold the
    // files of something else, so the run goes on.
    void
    testDirectoryWithoutEntry()
    {
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const root(td.path());
        std::string const dir = makeExtensionsDir(root);
        std::string const empty = makeExtensionDir(dir, "empty");
        std::string const unrelated = makeExtensionDir(dir, "unrelated");
        writeFile(files::appendPath(unrelated, "rules.lua"));
        writeFile(files::appendPath(unrelated, "other.js"));

        auto const found = collectExtensions({root}, reporter_);
        BOOST_TEST(found.has_value());
        BOOST_TEST((found && found->empty()));
    }

    // Two entry files in one directory leave it unclear what the extension
    // is, and the error names the directory and the candidates. The two
    // names of one language are the exception: the convention of the
    // language wins.
    void
    testDirectoryWithTwoCandidates()
    {
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const root(td.path());
        std::string const dir = makeExtensionsDir(root);

        // Two languages.
        std::string const mixed = makeExtensionDir(dir, "mixed");
        writeFile(files::appendPath(mixed, "mixed.lua"));
        writeFile(files::appendPath(mixed, "mixed.js"));
        auto found = collectExtensions({root}, reporter_);
        BOOST_TEST(!found.has_value());
        if (!found)
        {
            std::string const reason(found.error().reason());
            BOOST_TEST(reason.find("mixed") != std::string::npos);
            BOOST_TEST(reason.find("mixed.lua") != std::string::npos);
            BOOST_TEST(reason.find("mixed.js") != std::string::npos);
        }

        // The two names of one language: init.lua wins.
        std::error_code ec;
        BOOST_TEST(std::filesystem::remove(
            std::filesystem::path(files::appendPath(mixed, "mixed.js")), ec));
        writeFile(files::appendPath(mixed, "init.lua"));
        found = collectExtensions({root}, reporter_);
        BOOST_TEST(found.has_value());
        if (found)
        {
            BOOST_TEST(found->size() == 1u);
            BOOST_TEST((!found->empty() &&
                found->front().path.ends_with("/mixed/init.lua")));
        }

        // index.js wins over the name of the directory.
        std::string const scripts = makeExtensionDir(dir, "scripts");
        writeFile(files::appendPath(scripts, "scripts.js"));
        writeFile(files::appendPath(scripts, "index.js"));
        found = collectExtensions({root}, reporter_);
        BOOST_TEST(found.has_value());
        if (found)
        {
            BOOST_TEST(found->size() == 2u);
            BOOST_TEST(std::ranges::any_of(*found, [](ExtensionEntry const& e)
                { return e.path.ends_with("/scripts/index.js"); }));
            BOOST_TEST(std::ranges::none_of(*found, [](ExtensionEntry const& e)
                { return e.path.ends_with("/scripts/scripts.js"); }));
        }

        // Two library names stay an error. A platform that loads one
        // library name has no pair to hold.
        if (libraryExtension != ".dll")
        {
            std::string const report = makeExtensionDir(dir, "report");
            writeFile(files::appendPath(report, libraryName("report")));
            writeFile(files::appendPath(report, libraryName("libreport")));
            found = collectExtensions({root}, reporter_);
            BOOST_TEST(!found.has_value());
            if (!found)
            {
                std::string const reason(found.error().reason());
                BOOST_TEST(reason.find("libreport") != std::string::npos);
            }
        }
    }

    // Created out of order: the load order comes from the names, not from
    // the order the filesystem reports entries in, and it is the same for
    // scripts, native extensions and directories.
    void
    testNameOrderAcrossKinds()
    {
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const root(td.path());
        std::string const dir = makeExtensionsDir(root);
        writeFile(files::appendPath(dir, "d.js"));
        writeFile(files::appendPath(dir, libraryName("b")));
        writeFile(files::appendPath(dir, "e.lua"));
        std::string const named = makeExtensionDir(dir, "c");
        writeFile(files::appendPath(named, "init.lua"));
        std::string const natives = makeExtensionDir(dir, "a");
        writeFile(files::appendPath(natives, libraryName("a")));

        auto const found = collectExtensions({root}, reporter_);
        BOOST_TEST(found.has_value());
        if (!found)
        {
            return;
        }
        BOOST_TEST((fileNames(*found) == std::vector<std::string>{libraryName("a"), libraryName("b"), "init.lua", "d.js", "e.lua"}));
    }

    // Names compare as bytes, so an upper-case name comes before a
    // lower-case one, whatever the locale says.
    void
    testByteOrder()
    {
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const root(td.path());
        std::string const dir = makeExtensionsDir(root);
        writeFile(files::appendPath(dir, "a.lua"));
        writeFile(files::appendPath(dir, "B.lua"));

        auto const found = collectExtensions({root}, reporter_);
        BOOST_TEST(found.has_value());
        if (found)
        {
            BOOST_TEST((fileNames(*found) ==
                std::vector<std::string>{"B.lua", "a.lua"}));
        }
    }

    // The entries of every root form one list, in the order of their
    // names. An entry of the same name in a later root replaces the one
    // from an earlier root.
    void
    testLaterRootReplacesSameName()
    {
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const primary = files::appendPath(td.path(), "primary");
        std::string const supplemental =
            files::appendPath(td.path(), "supplemental");
        std::string const first = makeExtensionsDir(primary);
        std::string const second = makeExtensionsDir(supplemental);
        writeFile(files::appendPath(first, "brief-filler.lua"));
        writeFile(files::appendPath(second, "brief-filler.lua"));

        auto const found = collectExtensions({primary, supplemental}, reporter_);
        BOOST_TEST(found.has_value());
        if (!found || found->size() != 1)
        {
            BOOST_TEST((found && found->size() == 1));
            return;
        }
        BOOST_TEST((*found)[0].path == files::makePosixStyle(
            files::appendPath(second, "brief-filler.lua")));
    }

    // Entries with different names are all kept, in name order whichever
    // root they come from.
    void
    testDifferentNamesAcrossRootsAreKept()
    {
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const primary = files::appendPath(td.path(), "primary");
        std::string const supplemental =
            files::appendPath(td.path(), "supplemental");
        std::string const first = makeExtensionsDir(primary);
        std::string const second = makeExtensionsDir(supplemental);
        writeFile(files::appendPath(first, "zzz.lua"));
        writeFile(files::appendPath(first, "same-a.js"));
        writeFile(files::appendPath(second, "aaa.lua"));
        writeFile(files::appendPath(second, "same-b.lua"));

        auto const found = collectExtensions({primary, supplemental}, reporter_);
        BOOST_TEST(found.has_value());
        if (!found)
        {
            return;
        }
        BOOST_TEST((fileNames(*found) == std::vector<std::string>{"aaa.lua", "same-a.js", "same-b.lua", "zzz.lua"}));
    }

    // An extension is named by its stem, so a later root replaces an earlier
    // entry of the same name whatever the language: a .js replaces a .lua,
    // and a directory replaces a file.
    void
    testLaterRootReplacesSameStemOfAnotherKind()
    {
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const primary = files::appendPath(td.path(), "primary");
        std::string const supplemental =
            files::appendPath(td.path(), "supplemental");
        std::string const first = makeExtensionsDir(primary);
        std::string const second = makeExtensionsDir(supplemental);
        writeFile(files::appendPath(first, "brief-filler.lua"));
        writeFile(files::appendPath(first, "checks.lua"));
        writeFile(files::appendPath(second, "brief-filler.js"));
        writeFile(files::appendPath(
            makeExtensionDir(second, "checks"), "init.lua"));

        auto const found = collectExtensions({primary, supplemental}, reporter_);
        BOOST_TEST(found.has_value());
        if (!found || found->size() != 2)
        {
            BOOST_TEST((found && found->size() == 2));
            return;
        }
        BOOST_TEST((*found)[0].path == files::makePosixStyle(files::appendPath(second, "brief-filler.js")));
        BOOST_TEST((*found)[1].directory ==
            files::makePosixStyle(files::appendPath(second, "checks")));
    }

    // A native library is named without its `lib` prefix, so `libstats` in
    // a later root replaces `stats` from an earlier one.
    void
    testLaterRootReplacesLibraryWithOrWithoutPrefix()
    {
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const primary = files::appendPath(td.path(), "primary");
        std::string const supplemental =
            files::appendPath(td.path(), "supplemental");
        std::string const first = makeExtensionsDir(primary);
        std::string const second = makeExtensionsDir(supplemental);
        writeFile(files::appendPath(first, libraryName("stats")));
        writeFile(files::appendPath(second, libraryName("libstats")));

        auto const found = collectExtensions({primary, supplemental}, reporter_);
        BOOST_TEST(found.has_value());
        if (!found || found->size() != 1)
        {
            BOOST_TEST((found && found->size() == 1));
            return;
        }
        BOOST_TEST((*found)[0].path == files::makePosixStyle(
            files::appendPath(second, libraryName("libstats"))));
    }

    // A directory with a native entry is named without the `lib` prefix, like
    // a library file, so `libstats/` and `stats.<ext>` meet.
    void
    testNativeDirectoryLosesLibPrefix()
    {
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const primary = files::appendPath(td.path(), "primary");
        std::string const supplemental =
            files::appendPath(td.path(), "supplemental");
        std::string const first = makeExtensionsDir(primary);
        std::string const second = makeExtensionsDir(supplemental);
        writeFile(files::appendPath(first, libraryName("libstats")));
        std::string const dir = makeExtensionDir(second, "libstats");
        writeFile(files::appendPath(dir, libraryName("libstats")));

        auto const found = collectExtensions({primary, supplemental}, reporter_);
        BOOST_TEST(found.has_value());
        if (!found || found->size() != 1)
        {
            BOOST_TEST((found && found->size() == 1));
            return;
        }
        BOOST_TEST((*found)[0].directory == files::makePosixStyle(dir));
    }

    // On macOS the library suffixes `.so` and `.dylib` name one extension.
    void
    testLibrarySuffixesShareAName()
    {
#ifdef __APPLE__
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const primary = files::appendPath(td.path(), "primary");
        std::string const supplemental =
            files::appendPath(td.path(), "supplemental");
        std::string const first = makeExtensionsDir(primary);
        std::string const second = makeExtensionsDir(supplemental);
        writeFile(files::appendPath(first, "libstats.so"));
        writeFile(files::appendPath(second, "stats.dylib"));

        auto const found = collectExtensions({primary, supplemental}, reporter_);
        BOOST_TEST(found.has_value());
        BOOST_TEST((found && found->size() == 1));
        if (found && found->size() == 1)
        {
            BOOST_TEST(found->front().path.ends_with("/stats.dylib"));
        }
#endif
    }

    // Names ignore case where file names do, the `lib` prefix included.
    void
    testNamesIgnoreCaseWhereFileNamesDo()
    {
#if defined(_WIN32) || defined(__APPLE__)
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const primary = files::appendPath(td.path(), "primary");
        std::string const supplemental =
            files::appendPath(td.path(), "supplemental");
        std::string const first = makeExtensionsDir(primary);
        std::string const second = makeExtensionsDir(supplemental);
        writeFile(files::appendPath(first, "Brief-Filler.lua"));
        writeFile(files::appendPath(first, libraryName("LibStats")));
        writeFile(files::appendPath(second, "brief-filler.lua"));
        writeFile(files::appendPath(second, libraryName("stats")));

        auto const found = collectExtensions({primary, supplemental}, reporter_);
        BOOST_TEST(found.has_value());
        BOOST_TEST((found && found->size() == 2));
#endif
    }

    // Two entries of one directory with the same name are an error that
    // names the directory, the name and the entries.
    void
    testSameNameInOneDirectoryIsAnError()
    {
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const root(td.path());
        std::string const dir = makeExtensionsDir(root);
        writeFile(files::appendPath(dir, "brief-filler.lua"));
        writeFile(files::appendPath(dir, "brief-filler.js"));

        auto const found = collectExtensions({root}, reporter_);
        BOOST_TEST(!found.has_value());
        if (!found)
        {
            std::string const reason(found.error().reason());
            BOOST_TEST(reason.find(files::makePosixStyle(dir)) !=
                std::string::npos);
            BOOST_TEST(reason.find(
                           "more than one extension is named \"brief-filler\" "
                           "(brief-filler.js, brief-filler.lua)") != std::string::npos);
        }
    }

    // A script and a directory of one name in one directory are the same
    // error, and so are a library with and without the `lib` prefix.
    void
    testSameNameAcrossKindsInOneDirectoryIsAnError()
    {
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const root(td.path());
        std::string const dir = makeExtensionsDir(root);
        writeFile(files::appendPath(dir, "checks.lua"));
        writeFile(files::appendPath(
            makeExtensionDir(dir, "checks"), "init.lua"));
        auto const mixed = collectExtensions({root}, reporter_);
        BOOST_TEST(!mixed.has_value());
        if (!mixed)
        {
            BOOST_TEST(std::string(mixed.error().reason()).find(
                "(checks.lua, checks/)") != std::string::npos);
        }

        ScopedTempDirectory other("mrdocs-extensions");
        BOOST_TEST(other);
        std::string const otherRoot(other.path());
        std::string const otherDir = makeExtensionsDir(otherRoot);
        writeFile(files::appendPath(otherDir, libraryName("stats")));
        writeFile(files::appendPath(otherDir, libraryName("libstats")));
        BOOST_TEST(!collectExtensions({otherRoot}, reporter_).has_value());
    }

    // A directory extension is named by its directory, so a directory of
    // the same name in a later root replaces it, whatever entry file each
    // one holds.
    void
    testLaterRootReplacesSameNameDirectory()
    {
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const primary = files::appendPath(td.path(), "primary");
        std::string const supplemental =
            files::appendPath(td.path(), "supplemental");
        std::string const first =
            makeExtensionDir(makeExtensionsDir(primary), "checks");
        std::string const second =
            makeExtensionDir(makeExtensionsDir(supplemental), "checks");
        writeFile(files::appendPath(first, "init.lua"));
        writeFile(files::appendPath(second, "index.js"));

        auto const found = collectExtensions({primary, supplemental}, reporter_);
        BOOST_TEST(found.has_value());
        if (!found || found->size() != 1)
        {
            BOOST_TEST((found && found->size() == 1));
            return;
        }
        BOOST_TEST((*found)[0].kind == ExtensionKind::JavaScript);
        BOOST_TEST((*found)[0].directory == files::makePosixStyle(second));
    }

    // A library entry replaces the library of the same file name in an
    // earlier root, so a plugin built by the project replaces a base one.
    void
    testLaterRootReplacesSameNameLibrary()
    {
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const primary = files::appendPath(td.path(), "primary");
        std::string const supplemental =
            files::appendPath(td.path(), "supplemental");
        std::string const first = makeExtensionsDir(primary);
        std::string const second = makeExtensionsDir(supplemental);
        writeFile(files::appendPath(first, libraryName("libstats")));
        writeFile(files::appendPath(second, libraryName("libstats")));

        auto const found = collectExtensions({primary, supplemental}, reporter_);
        BOOST_TEST(found.has_value());
        if (!found || found->size() != 1)
        {
            BOOST_TEST((found && found->size() == 1));
            return;
        }
        BOOST_TEST((*found)[0].path == files::makePosixStyle(
            files::appendPath(second, libraryName("libstats"))));
    }

    // A configuration can name one root twice, directly or through a link.
    // Loading a library again would run its entry point a second time and
    // fail on the id it already installed.
    void
    testRepeatedRootLoadsNativeOnce()
    {
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const root(td.path());
        std::string const dir = makeExtensionsDir(root);
        writeFile(files::appendPath(dir, libraryName("stats")));
        std::string const named = makeExtensionDir(dir, "report");
        writeFile(files::appendPath(named, libraryName("report")));

        auto const found = collectExtensions({root, root}, reporter_);
        BOOST_TEST(found.has_value());
        BOOST_TEST((found && found->size() == 2));
    }

    // The same, with the library reached by a root that is spelled
    // differently: the filesystem decides which files are the same one.
    void
    testRepeatedRootSpelledDifferently()
    {
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const root(td.path());
        writeFile(files::appendPath(makeExtensionsDir(root),
            libraryName("stats")));
        std::string const indirect =
            files::appendPath(files::appendPath(root, "."), "");

        auto const found = collectExtensions({root, indirect}, reporter_);
        BOOST_TEST(found.has_value());
        BOOST_TEST((found && found->size() == 1));
    }

    // A library reached under two names across roots is one file, and
    // loading it twice would run its entry point twice.
    void
    testLinkedLibraryUnderTwoNames()
    {
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const primary = files::appendPath(td.path(), "primary");
        std::string const supplemental =
            files::appendPath(td.path(), "supplemental");
        std::string const first = makeExtensionsDir(primary);
        std::string const second = makeExtensionsDir(supplemental);
        std::string const target =
            files::appendPath(first, libraryName("libstats"));
        writeFile(target);
        std::error_code ec;
        std::filesystem::create_symlink(
            std::filesystem::path(target),
            std::filesystem::path(files::appendPath(second, libraryName("other"))), ec);
        if (ec)
        {
            return;
        }

        auto const found = collectExtensions({primary, supplemental}, reporter_);
        BOOST_TEST(found.has_value());
        BOOST_TEST((found && found->size() == 1));
    }

    // A script reached twice through one root has one name, so it is
    // reported once.
    void
    testRepeatedRootKeepsOneScript()
    {
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const root(td.path());
        writeFile(files::appendPath(makeExtensionsDir(root), "a.lua"));

        auto const found = collectExtensions({root, root}, reporter_);
        BOOST_TEST(found.has_value());
        BOOST_TEST((found && found->size() == 1));
    }

    // A root whose path is not ASCII is recognized when it is named twice,
    // which takes reading its path as UTF-8 rather than in the ANSI code
    // page Windows would otherwise use.
    void
    testNonAsciiRepeatedRoot()
    {
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        // An e with an acute accent, as its UTF-8 bytes.
        std::string const root =
            files::appendPath(td.path(), "Jos\xC3\xA9");
        writeFile(files::appendPath(makeExtensionsDir(root),
            libraryName("stats")));

        auto const found = collectExtensions({root, root}, reporter_);
        BOOST_TEST(found.has_value());
        BOOST_TEST((found && found->size() == 1));
    }

    // The addon roots are UTF-8 text. A root with characters outside the
    // ANSI code page of Windows is found, and the extensions in it come
    // back under the same UTF-8 spelling.
    void
    testNonAsciiRoot()
    {
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const parent(td.path());
        std::string const root = files::appendPath(
            parent, "addons-Jos\xC3\xA9-\xE6\x97\xA5\xE6\x9C\xAC");
        std::string const stem = "plug-\xC3\xA9\xE6\x97\xA5";
        std::string const dir = makeExtensionsDir(root);
        std::string const library =
            files::appendPath(dir, libraryName(stem));
        writeFile(library);
        auto const found = collectExtensions({root}, reporter_);
        BOOST_TEST(found.has_value());
        BOOST_TEST((found && found->size() == 1));
        if (found && !found->empty())
        {
            // Discovery reports paths with forward slashes, like every
            // path MrDocs reports.
            BOOST_TEST(found->front().path == files::makePosixStyle(library));
        }
    }

    // CMake gives a module library the .so extension on macOS, so both
    // spellings have to be found there.
    void
    testAppleExtensions()
    {
#ifdef __APPLE__
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const root(td.path());
        std::string const dir = makeExtensionsDir(root);
        writeFile(files::appendPath(dir, "bundle.so"));
        writeFile(files::appendPath(dir, "library.dylib"));

        auto const found = collectExtensions({root}, reporter_);
        BOOST_TEST(found.has_value());
        BOOST_TEST((found && found->size() == 2));
#endif
    }

    // A directory carries the libraries of the platforms it supports side
    // by side: the ones this platform does not load are not candidates.
    void
    testOtherPlatformLibrariesAreIgnored()
    {
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const root(td.path());
        std::string const dir = makeExtensionsDir(root);
        std::string const named = makeExtensionDir(dir, "report");
        writeFile(files::appendPath(named, libraryName("report")));
        for (std::string_view other : {".so", ".dylib", ".dll"})
        {
            if (other != libraryExtension)
            {
#ifdef __APPLE__
                if (other == ".so")
                {
                    continue;
                }
#endif
                writeFile(files::appendPath(
                    named, "report" + std::string(other)));
            }
        }

        auto const found = collectExtensions({root}, reporter_);
        BOOST_TEST(found.has_value());
        BOOST_TEST((found && found->size() == 1));
    }

    // Windows and macOS ignore case in file names by default, so an
    // upper-case extension still names a library there. Elsewhere the name
    // stays as it is.
    void
    testExtensionCase()
    {
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const root(td.path());
        std::string const dir = makeExtensionsDir(root);
        std::string extension(libraryExtension);
        std::ranges::transform(extension, extension.begin(),
            [](unsigned char c)
            {
                return static_cast<char>(std::toupper(c));
            });
        writeFile(files::appendPath(dir, "STATS" + extension));

        auto const found = collectExtensions({root}, reporter_);
        BOOST_TEST(found.has_value());
#if defined(_WIN32) || defined(__APPLE__)
        BOOST_TEST((found && found->size() == 1));
#else
        BOOST_TEST((found && found->empty()));
#endif
    }

    // An `extensions` entry that is a file, not a directory, holds no
    // extension: the run goes on after saying so.
    void
    testExtensionsIsAFile()
    {
        ScopedTempDirectory td("mrdocs-extensions");
        BOOST_TEST(td);
        std::string const root(td.path());
        writeFile(files::appendPath(root, "extensions"));

        auto const found = collectExtensions({root}, reporter_);
        BOOST_TEST(found.has_value());
        BOOST_TEST((found && found->empty()));
    }

    void
    run()
    {
        testRootWithoutExtensionsDir();
        testMissingRoot();
        testFileEntriesOfAllKinds();
        testDirectoryEntries();
        testDirectoryNamedInit();
        testDirectoryNamedInitDifferingInCase();
        testDirectoryWithoutEntry();
        testDirectoryWithTwoCandidates();
        testNameOrderAcrossKinds();
        testByteOrder();
        testLaterRootReplacesSameName();
        testDifferentNamesAcrossRootsAreKept();
        testLaterRootReplacesSameNameDirectory();
        testLaterRootReplacesSameNameLibrary();
        testLaterRootReplacesSameStemOfAnotherKind();
        testLaterRootReplacesLibraryWithOrWithoutPrefix();
        testNativeDirectoryLosesLibPrefix();
        testLibrarySuffixesShareAName();
        testNamesIgnoreCaseWhereFileNamesDo();
        testSameNameInOneDirectoryIsAnError();
        testSameNameAcrossKindsInOneDirectoryIsAnError();
        testRepeatedRootLoadsNativeOnce();
        testRepeatedRootSpelledDifferently();
        testLinkedLibraryUnderTwoNames();
        testRepeatedRootKeepsOneScript();
        testNonAsciiRepeatedRoot();
        testNonAsciiRoot();
        testAppleExtensions();
        testOtherPlatformLibrariesAreIgnored();
        testExtensionCase();
        testExtensionsIsAFile();
    }
};

TEST_SUITE(
    AddonDiscoveryTest,
    "clang.mrdocs.AddonDiscovery");

} // mrdocs
