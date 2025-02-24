//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "TestArgs.hpp"
#include "TestRunner.hpp"
#include "lib/Support/Debug.hpp"
#include "lib/Support/Report.hpp"
#include <mrdocs/Platform.hpp>
#include <mrdocs/Version.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Path.hpp>
#include <test_suite/test_suite.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>
#include <stdlib.h>

int main(int argc, char const** argv);

namespace clang {
namespace mrdocs {

void DoTestAction(char const** argv)
{
    using namespace clang::mrdocs;

    std::vector<std::string> testPaths(
        testArgs.cmdLineInputs.begin(),
        testArgs.cmdLineInputs.end());
    testArgs.cmdLineInputs.clear();
    for (auto const& inputPath: testPaths)
    {
        if (!files::exists(inputPath))
        {
            report::warn("Path does not exist: \"{}\"", inputPath);
        }
    }

    TestRunner runner(testArgs.generator);
    for (auto const& inputPath: testPaths)
    {
        runner.checkPath(inputPath, argv);
    }
    auto const& results = runner.results;

    std::stringstream os;
    switch(testArgs.action)
    {
    case Action::test:
        os << "Test action: ";
        break;
    case Action::create:
        os << "Create action: ";
        break;
    case Action::update:
        os << "Update action: ";
        break;
    default:
        MRDOCS_UNREACHABLE();
    }

    os <<
        report::numberOf(results.numberOfDirs.load(),
        "directory", "directories") << " visited";
    if (auto n = results.expectedDocsMatching.load())
    {
        os << ", " << report::numberOf(n, "file", "files") << " matched";
    }
    if (auto n = results.expectedDocsWritten.load())
    {
        os << ", " << report::numberOf(n, "file", "files") << " written";
    }
    os << ".\n";
    report::print(os.str());
}

int test_main(int argc, char const** argv)
{
    // VFALCO this heap checking is too strong for
    // a clang tool's model of what is actually a leak.
    // debugEnableHeapChecking();

    llvm::EnablePrettyStackTrace();
    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
    llvm::setBugReportMsg("PLEASE submit a bug report to https://github.com/cppalliance/mrdocs/issues/ and include the crash backtrace.\n");

    testArgs.hideForeignOptions();
    if (!llvm::cl::ParseCommandLineOptions(argc, argv, testArgs.usageText))
    {
        return EXIT_FAILURE;
    }

    // Apply log-level
    auto ll = PublicSettings::LogLevel::Info;
    PublicSettings::fromString(testArgs.logLevel.getValue(), ll);
    report::setMinimumLevel(static_cast<report::Level>(ll));
    report::setSourceLocationWarnings(false);

    if (!testArgs.cmdLineInputs.empty())
    {
        DoTestAction(argv);
    }

    if (testArgs.unitOption.getValue())
    {
        test_suite::unit_test_main(argc, argv);
    }

    if (report::results.errorCount > 0 ||
        report::results.fatalCount > 0)
    {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

#ifdef _NDEBUG
static void reportUnhandledException(
    std::exception const& ex)
{
    namespace sys = llvm::sys;

    report::error("Unhandled exception: {}\n", ex.what());
    sys::PrintStackTrace(llvm::errs());
}
#endif

} // mrdocs
} // clang

int main(int argc, char const** argv)
{
#ifndef _NDEBUG
    return clang::mrdocs::test_main(argc, argv);
#else
    try
    {
        return clang::mrdocs::test_main(argc, argv);
    }
    catch(clang::mrdocs::Exception const& ex)
    {
        // thrown Exception should never get here.
        clang::mrdocs::reportUnhandledException(ex);
    }
    catch(std::exception const& ex)
    {
        clang::mrdocs::reportUnhandledException(ex);
    }
    return EXIT_FAILURE;
#endif
}
