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

#include <mrdocs/Config.hpp>
#include <mrdocs/Extensions/ExtensionRegistry.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/MrDocsCompilationDatabase.hpp>
#include <mrdocs/Session.hpp>
#include <mrdocs/Support/Concurrency/ThreadPool.hpp>
#include <mrdocs/Support/Reporter.hpp>
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

    // The reporter of the runner: the settings it loads and the fixtures
    // that fail report here.
    report::Reporter& reporter_;

    // The native libraries the fixtures load, shared by every session of
    // the runner and by the threads that run them: a library is loaded and
    // its entry point run once per process.
    std::shared_ptr<ExtensionRegistry::LibraryRegistry> libraries_ =
        std::make_shared<ExtensionRegistry::LibraryRegistry>();

public:
    TestResults results;

    /** Construct a runner.

        @param reporter The reporter of the runner. Each fixture runs in
        sessions of its own, which take the level of this reporter.
    */
    explicit TestRunner(report::Reporter& reporter)
        : reporter_(reporter)
    {
    }

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

    /** Give a session of a fixture the settings of the runner's reporter. */
    void
    configureLog(Session& session) const;

    /** Fail a fixture whose session reported an error, as the tool fails
        a run that does.
    */
    static Expected<void>
    checkReported(Session const& session);

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
