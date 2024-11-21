//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_TEST_TESTRUNNER_HPP
#define MRDOCS_TEST_TESTRUNNER_HPP

#include "lib/Lib/ConfigImpl.hpp"
#include <mrdocs/Generator.hpp>
#include <mrdocs/Support/ThreadPool.hpp>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/ErrorOr.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>

namespace clang {
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
    llvm::ErrorOr<std::string> diffCmdPath_;
    Generator const* gen_;
    ReferenceDirectories dirs_;

    Expected<void>
    writeFile(
        llvm::StringRef filePath,
        llvm::StringRef contents);

    void
    handleFile(
        llvm::StringRef filePath,
        Config::Settings const& dirSettings);

    void
    handleDir(
        std::string dirPath,
        Config::Settings const& dirSettings);

public:
    TestResults results;

    TestRunner(std::string_view generator);

    /** Check a single file, or a directory recursively.

        This function checks the specified path
        and blocks until completed.
    */
    void checkPath(std::string inputPath, char const** argv);
};

} // mrdocs
} // clang

#endif
