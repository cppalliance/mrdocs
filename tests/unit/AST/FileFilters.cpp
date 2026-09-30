//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Config.hpp>
#include <mrdocs/Config/ReferenceDirectories.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <mrdocs/Support/Filesystem/Temp.hpp>
#include <test_suite/test_suite.hpp>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>

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

// Lay out `<root>/api/input.cpp`, which reaches `<root>/vendor/vendored.hpp`
// through `#include "../vendor/vendored.hpp"`.
void
writeTree(std::string const& rootDir)
{
    std::string const apiDir = files::appendPath(rootDir, "api");
    std::string const vendorDir = files::appendPath(rootDir, "vendor");
    BOOST_TEST(files::createDirectory(apiDir).has_value());
    BOOST_TEST(files::createDirectory(vendorDir).has_value());
    writeFile(files::appendPath(apiDir, "input.cpp"),
        "#include \"../vendor/vendored.hpp\"\n"
        "struct Documented {};\n");
    writeFile(files::appendPath(vendorDir, "vendored.hpp"),
        "namespace vendor { struct Vendored {}; }\n");
}

// Whether `vendor::Vendored` is extracted when the tree is documented with
// `filters` added to its configuration. The configuration uses absolute
// paths, so no config-relative placeholders need resolving; `addons`
// points back at the tree only to satisfy the option's existence check.
bool
extractsVendored(std::string const& rootDir, std::string const& filters)
{
    std::string const configPath = files::appendPath(rootDir, "mrdocs.yml");
    writeFile(configPath,
        "source-root: " + rootDir + "\n"
        "addons: " + rootDir + "\n"
        "extract-all: true\n"
        "warn-if-undocumented: false\n" + filters);

    ReferenceDirectories dirs;
    dirs.cwd = rootDir;
    dirs.mrdocsRoot = rootDir;
    Config config;
    BOOST_TEST(Config::load_file(config, configPath, dirs).has_value());
    Expected<Corpus> corpus = Corpus::build(config);
    BOOST_TEST(corpus.has_value());
    bool result = false;
    if (corpus)
    {
        BOOST_TEST(corpus->lookup(SymbolID::global, "Documented").has_value());
        result = corpus->lookup(
            SymbolID::global, "vendor::Vendored").has_value();
    }
    return result;
}

} // (anon)

// Which files count as inputs and which are excluded, checked over a real
// corpus. The golden harness cannot cover this: it hands the compiler a
// fixture by a relative name, so every path the compiler records is
// relative, and MrDocs normalizes a relative path when it makes it
// absolute. A compilation database gives the compiler absolute names, and
// so does the database the configuration synthesizes, which is what these
// tests build.
//
// In both tests the header is recorded as the unnormalized path
// `<root>/api/../vendor/vendored.hpp`, which reads as though it lay inside
// `<root>/api` and outside `<root>/vendor`, the opposite of where it is.
struct FileFilters_test
{
    void
    testUnnormalizedPathOutsideTheInputs()
    {
        ScopedTempDirectory root("mrdocs-filefilters");
        BOOST_TEST(root);
        std::string const rootDir(root.path());
        writeTree(rootDir);
        BOOST_TEST(!extractsVendored(rootDir,
            "input:\n  - " + files::appendPath(rootDir, "api") + "\n"));
    }

    void
    testUnnormalizedPathInsideAnExclusion()
    {
        ScopedTempDirectory root("mrdocs-filefilters");
        BOOST_TEST(root);
        std::string const rootDir(root.path());
        writeTree(rootDir);
        BOOST_TEST(!extractsVendored(rootDir,
            "input:\n  - " + rootDir + "\n"
            "exclude:\n  - " + files::appendPath(rootDir, "vendor") + "\n"));
    }

    void
    run()
    {
        testUnnormalizedPathOutsideTheInputs();
        testUnnormalizedPathInsideAnExclusion();
    }
};

TEST_SUITE(
    FileFilters_test,
    "clang.mrdocs.FileFilters");

} // mrdocs
