//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_TEST_TESTRUNNER_HPP
#define MRDOX_TEST_TESTRUNNER_HPP

#include "Tool/ConfigImpl.hpp"
#include <mrdox/Generator.hpp>
#include <mrdox/Support/ThreadPool.hpp>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/ErrorOr.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>

namespace clang {
namespace mrdox {

struct TestResults
{
    using clock_type = std::chrono::system_clock;

    clock_type::time_point startTime;
    std::atomic<std::size_t> numberOfDirs = 0;
    std::atomic<std::size_t> numberOfFiles = 0;
    std::atomic<std::size_t> numberOfErrors = 0;
    std::atomic<std::size_t> numberOfFailures = 0;
    std::atomic<std::size_t> numberofFilesWritten = 0;

    TestResults() noexcept
        : startTime(clock_type::now())
    {
    }

    // Return the number of milliseconds of elapsed time
    auto
    elapsedMilliseconds() const noexcept
    {
        return std::chrono::duration_cast<
            std::chrono::milliseconds>(
                clock_type::now() - startTime).count();
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
    Generator const* xmlGen_;

    Error writeFile(
        llvm::StringRef filePath,
        llvm::StringRef contents);

    Error handleFile(
        llvm::StringRef filePath,
        std::shared_ptr<ConfigImpl const> config);

    Error handleDir(
        std::string dirPath,
        std::shared_ptr<ConfigImpl const> config);

public:
    TestResults results;

    TestRunner();

    /** Check a single file, or a directory recursively.

        This function checks the specified path
        and blocks until completed.
    */
    Error checkPath(std::string inputPath);
};

} // mrdox
} // clang

#endif
