//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_TEST_TESTRUNNER_HPP
#define MRDOCS_TEST_TESTRUNNER_HPP

#include <lib/ConfigImpl.hpp>
#include <lib/MrDocsCompilationDatabase.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Support/ThreadPool.hpp>
#include <clang/Tooling/CompilationDatabase.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/ErrorOr.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <test/Support/TestLayout.hpp>


namespace mrdocs {

struct TestResults
{
    // Number of expected doc files written
    std::atomic<std::size_t> expectedDocsWritten = 0;

    // Number of matching expected doc files
    std::atomic<std::size_t> expectedDocsMatching = 0;

    // Number of directories visited
    std::atomic<std::size_t> numberOfDirs = 0;

    TestResults() noexcept
    {
    }
};

//------------------------------------------------

// We need a different config for each directory
// or file passed on the command line, and thus
// each input path must have a separate TestRunner.

/** Runs tests on a file or directory.
*/
class TestRunner
{
    ThreadPool threadPool_;
    /// Id of the chosen generator. Resolved per-test (after each test's
    /// settings load) so that data-driven generators contributed via
    /// addons-supplemental are picked up correctly.
    std::string genId_;
    ReferenceDirectories dirs_;

    /** Run a single .cpp test file with inherited directory settings. */
    void
    handleFile(
        llvm::StringRef filePath,
        Config::Settings const& dirSettings);

    /** Traverse a directory, applying configs and enqueueing .cpp tests. */
    void
    handleDir(
        std::string dirPath,
        Config::Settings const& dirSettings);

public:
    TestResults results;

    /** Construct a runner for the chosen generator id. */
    TestRunner(std::string_view generator);

    /** Execute a compilation/database run for one test input. */
    void
    handleCompilationDatabase(
        llvm::StringRef filePath,
        Generator const& gen,
        MrDocsCompilationDatabase const& compilations,
        std::shared_ptr<ConfigImpl const> const& config,
        TestLayout const& layout);

    /** Check a single file, or a directory recursively.

        This function checks the specified path
        and blocks until completed.
    */
    void checkPath(std::string inputPath, char const** argv);
};

} // mrdocs


#endif // MRDOCS_TEST_TESTRUNNER_HPP
