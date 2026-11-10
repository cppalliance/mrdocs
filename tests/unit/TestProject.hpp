//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// The scratch project the unit tests that run extensions build.

#ifndef MRDOCS_TEST_UNIT_TESTPROJECT_HPP
#define MRDOCS_TEST_UNIT_TESTPROJECT_HPP

#include <mrdocs/Config.hpp>
#include <mrdocs/Config/ReferenceDirectories.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <mrdocs/Support/Filesystem/Temp.hpp>
#include <mrdocs/Support/Reporter.hpp>
#include <test_suite/test_suite.hpp>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

namespace mrdocs {

/** A scratch project on disk and the configuration loaded for it.

    The source directory holds `input.cpp`, the addon
    directory holds the scripts under `extensions/`, and the output
    directory is where `config.output` points. The directories are removed
    with the object.
*/
struct TestProject
{
    ScopedTempDirectory src;
    ScopedTempDirectory addons;
    ScopedTempDirectory out;
    Config config;
};

/** Build a scratch project and load its configuration.

    The source is `struct Widget {};` and pulls in no headers, so extraction
    needs no toolchain resource directory.

    @param prefix The prefix of the names of the three directories.
    @param scripts The scripts to write under `extensions/` of the addon
    directory, each a file name and its text.
    @param reporter The reporter the configuration loads with.
*/
template <class Scripts = std::initializer_list<
    std::pair<std::string_view, std::string_view>>>
TestProject
makeTestProject(
    std::string_view const prefix,
    Scripts const& scripts,
    report::Reporter& reporter)
{
    TestProject project{
        ScopedTempDirectory(std::string(prefix) + "-src"),
        ScopedTempDirectory(std::string(prefix) + "-addons"),
        ScopedTempDirectory(std::string(prefix) + "-out"),
        Config()};
    BOOST_TEST(project.src);
    BOOST_TEST(project.addons);
    BOOST_TEST(project.out);
    std::string const srcDir(project.src.path());
    std::string const addonsDir(project.addons.path());
    BOOST_TEST(files::writeFileText(files::appendPath(srcDir, "input.cpp"),
        "struct Widget {};\n")
            .has_value());
    std::string const extensions = files::appendPath(addonsDir, "extensions");
    BOOST_TEST(files::createDirectory(extensions).has_value());
    for (auto const& [name, text] : scripts)
    {
        BOOST_TEST(files::writeFileText(
            files::appendPath(extensions, std::string_view(name)),
            std::string_view(text))
                .has_value());
    }
    // The configuration comes from a string that stands for a `mrdocs.yml`
    // in the source directory, which is not written, with absolute paths so
    // no config-relative placeholders need resolving.
    std::string const configYaml =
        "source-root: " + srcDir + "\n"
        "addons: " + addonsDir + "\n"
        "input:\n  - " + srcDir + "\n";
    ReferenceDirectories dirs;
    dirs.cwd = srcDir;
    dirs.mrdocsRoot = srcDir;
    BOOST_TEST(Config::loadString(project.config, configYaml,
        files::appendPath(srcDir, "mrdocs.yml"), dirs, reporter)
        .has_value());
    project.config.output = std::string(project.out.path());
    return project;
}

} // mrdocs

#endif
