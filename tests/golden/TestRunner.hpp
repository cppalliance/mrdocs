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

#ifndef MRDOCS_TEST_GOLDEN_TESTRUNNER_HPP
#define MRDOCS_TEST_GOLDEN_TESTRUNNER_HPP

#include <mrdocs/MrDocsCompilationDatabase.hpp>
#include <mrdocs/Config.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Support/Concurrency/ThreadPool.hpp>
#include <clang/Tooling/CompilationDatabase.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/ErrorOr.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>


namespace mrdocs {

struct TestResults
{
    // Number of expected doc files written
    std::atomic<std::size_t> expectedDocsWritten = 0;

    // Number of matching expected doc files
    std::atomic<std::size_t> expectedDocsMatching = 0;

    // Number of directories visited
    std::atomic<std::size_t> numberOfDirs = 0;
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

public:
    TestResults results;

    /// Construct a runner.
    TestRunner() = default;

    /** Check a single file, or a directory recursively.

        This function checks the specified path
        and blocks until completed.
    */
    Expected<void>
    checkPath(std::string inputPath, char const** argv);

private:
    /** Traverse a directory, applying configs and enqueueing .cpp tests. */
    Expected<void>
    handleDir(
        std::string dirPath,
        Config const& dirSettings,
        ReferenceDirectories const& dirs);

    /** Run a single .cpp test file with inherited directory settings. */
    Expected<void>
    handleFile(
        llvm::StringRef filePath,
        Config const& dirSettings,
        ReferenceDirectories const& dirs);

    /** Execute a compilation/database run for one test input. */
    Expected<void>
    handleCompilationDatabase(
        llvm::StringRef filePath,
        Generator const& gen,
        MrDocsCompilationDatabase const& compilations,
        Config const& config);
};

} // mrdocs


#endif // MRDOCS_TEST_GOLDEN_TESTRUNNER_HPP
