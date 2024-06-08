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
#include "lib/Support/Error.hpp"
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

int main(int argc, char** argv);

namespace clang {
namespace mrdocs {

void DoTestAction()
{
    using namespace clang::mrdocs;

    TestRunner runner;
    for(auto const& inputPath : testArgs.inputPaths)
        runner.checkPath(inputPath);
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
    }

    os <<
        report::numberOf(results.numberOfDirs.load(),
        "directory", "directories") << " visited";
    if(auto n = results.expectedXmlMatching.load())
        os << ", " << report::numberOf(n, "file", "files") << " matched";
    if(auto n = results.expectedXmlWritten.load())
        os << ", " << report::numberOf(n, "file", "files") << " written";
    os << ".\n";
    report::print(os.str());
}

int test_main(int argc, char const* const* argv)
{
    // VFALCO this heap checking is too strong for
    // a clang tool's model of what is actually a leak.
    // debugEnableHeapChecking();

    llvm::EnablePrettyStackTrace();
    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
    llvm::setBugReportMsg("PLEASE submit a bug report to https://github.com/cppalliance/mrdocs/issues/ and include the crash backtrace.\n");

    testArgs.hideForeignOptions();
    if(! llvm::cl::ParseCommandLineOptions(
            argc, argv, testArgs.usageText))
        return EXIT_FAILURE;

    // Apply reportLevel
    report::setMinimumLevel(report::getLevel(
        testArgs.reportLevel.getValue()));

    if(! testArgs.inputPaths.empty())
        DoTestAction();

    if(testArgs.unitOption.getValue())
        test_suite::unit_test_main(argc, argv);

    if( report::results.errorCount > 0 ||
        report::results.fatalCount > 0)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

static void reportUnhandledException(
    std::exception const& ex)
{
    namespace sys = llvm::sys;

    report::error("Unhandled exception: {}\n", ex.what());
    sys::PrintStackTrace(llvm::errs());
}

} // mrdocs
} // clang

int main(int argc, char** argv)
{
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
}
