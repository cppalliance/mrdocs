//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Platform.hpp>
#include <mrdocs/ConfigSchema.hpp>
#include <mrdocs/Support/Debug.hpp>
#include <mrdocs/Support/ReportImpl.hpp>
#include "TestCliArgs.hpp"
#include "TestRunner.hpp"
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <mrdocs/Version.hpp>
#include <test_suite/test_suite.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/raw_ostream.h>
#include <sstream>
#include <stdlib.h>

int main(int argc, char const** argv);


namespace mrdocs {

void DoTestAction(char const** argv)
{
    using namespace mrdocs;

    // The test paths are the positional arguments; configuration options were
    // recognized during parsing and left in argv for Config::load to apply.
    std::vector<std::string> const& testPaths = testCliArgs.inputs;
    for (auto const& inputPath: testPaths)
    {
        if (!files::exists(inputPath))
        {
            report::warn("Path does not exist: \"{}\"", inputPath);
        }
    }

    TestRunner runner;
    for (auto const& inputPath: testPaths)
    {
        if (auto r = runner.checkPath(inputPath, argv); !r)
        {
            report::error("{}: \"{}\"", r.error(), inputPath);
        }
    }
    auto const& results = runner.results;

    std::stringstream os;
    switch(testCliArgs.action)
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

    // Parse the harness's own options and test paths straight from argv;
    // configuration options are left in argv for Config::load to apply.
    testCliArgs = parseTestCommandLine(argc, argv);
    if (testCliArgs.showHelp)
    {
        llvm::outs() <<
R"(USAGE: mrdocs-test [options] ( dir | file )...

  --action=test|create|update  Compare, create, or rewrite fixtures.
  --bad                        Write a .bad.<ext> file for each failure.
  --force                      With update, rewrite even when normalized output matches.
  --log-level=<level>          Reporting level (trace|debug|info|warn|error|fatal).
  --help                       Print this help and exit.

Configuration options (for example --addons, --stdlib-includes) are also
accepted and applied like the mrdocs tool.
)";
        return EXIT_SUCCESS;
    }

    // Apply log-level
    auto ll = ConfigSchema::LogLevel::Info;
    ConfigSchema::fromString(testCliArgs.logLevel, ll);
    report::setMinimumLevel(static_cast<report::Level>(ll));
    report::setSourceLocationWarnings(false);

    if (!testCliArgs.inputs.empty())
    {
        DoTestAction(argv);
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


int main(int argc, char const** argv)
{
#ifndef _NDEBUG
    return mrdocs::test_main(argc, argv);
#else
    try
    {
        return mrdocs::test_main(argc, argv);
    }
    catch(mrdocs::Exception const& ex)
    {
        // thrown Exception should never get here.
        mrdocs::reportUnhandledException(ex);
    }
    catch(std::exception const& ex)
    {
        mrdocs::reportUnhandledException(ex);
    }
    return EXIT_FAILURE;
#endif
}
