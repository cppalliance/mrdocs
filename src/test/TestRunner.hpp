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
    // Number of expected XML files written
    std::atomic<std::size_t> expectedXmlWritten = 0;

    // Number of matching expected XML files
    std::atomic<std::size_t> expectedXmlMatching= 0;

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
    Generator const* xmlGen_;

    Error writeFile(
        llvm::StringRef filePath,
        llvm::StringRef contents);

    void handleFile(
        llvm::StringRef filePath,
        std::shared_ptr<ConfigImpl const> config);

    void handleDir(
        std::string dirPath,
        std::shared_ptr<ConfigImpl const> config);

public:
    TestResults results;

    TestRunner();

    /** Check a single file, or a directory recursively.

        This function checks the specified path
        and blocks until completed.
    */
    void checkPath(std::string inputPath);
};

} // mrdocs
} // clang

#endif
