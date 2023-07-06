//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "test_suite.hpp"
#include "TestArgs.hpp"
#include "TestRunner.hpp"
#include "Tool/Addons.hpp"
#include "Support/Debug.hpp"
#include <mrdox/Platform.hpp>
#include <mrdox/Version.hpp>
#include <mrdox/Support/Error.hpp>
#include <mrdox/Support/Path.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>
#include <stdlib.h>

int main(int argc, char** argv);

namespace clang {
namespace mrdox {

int
DoTestAction()
{
    using namespace clang::mrdox;

    TestRunner runner;
    for(auto const& inputPath : testArgs.inputPaths)
        if(auto err = runner.checkPath(inputPath))
            reportError(err, "check path \"{}\"", inputPath);

    auto& os = debug_outs();
    if(runner.results.numberofFilesWritten > 0)
        os <<
            runner.results.numberofFilesWritten << " files written\n";
    os <<
        "Checked " <<
        runner.results.numberOfFiles << " files (" <<
        runner.results.numberOfDirs << " dirs)";
    if( runner.results.numberOfErrors > 0 ||
        runner.results.numberOfFailures > 0)
    {
        if( runner.results.numberOfErrors > 0)
        {
            os <<
                ", with " <<
                runner.results.numberOfErrors << " errors";
            if(runner.results.numberOfFailures > 0)
                os <<
                    " and " << runner.results.numberOfFailures <<
                    " failures";
        }
        else
        {
            os <<
                ", with " <<
                runner.results.numberOfFailures <<
                " failures";
        }
    }
    auto milli = runner.results.elapsedMilliseconds();
    if(milli < 10000)
        os <<
            " in " << milli << " milliseconds\n";
#if 0
    else if(milli < 10000)
        os <<
            " in " << std::setprecision(1) <<
            double(milli) / 1000 << " seconds\n";
#endif
    else
        os <<
            " in " << ((milli + 500) / 1000) <<
            " seconds\n";

    if( runner.results.numberOfFailures > 0 ||
        runner.results.numberOfErrors > 0)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

int test_main(int argc, char const* const* argv)
{
    // VFALCO this heap checking is too strong for
    // a clang tool's model of what is actually a leak.
    // debugEnableHeapChecking();

    llvm::EnablePrettyStackTrace();
    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);

    if(! setupAddonsDir(testArgs.addonsDir, argv[0],
            reinterpret_cast<void*>(&main)))
        return EXIT_FAILURE;

    testArgs.hideForeignOptions();
    if(! llvm::cl::ParseCommandLineOptions(
            argc, argv, testArgs.usageText))
        return EXIT_FAILURE;

    int failure = 0;

    if(! testArgs.inputPaths.empty())
        failure |= DoTestAction();

    if(testArgs.unitOption.getValue())
        failure |= test_suite::unit_test_main(argc, argv);

    return failure;
}

} // mrdox
} // clang

int main(int argc, char** argv)
{
    try
    {
        return clang::mrdox::test_main(argc, argv);
    }
    catch(clang::mrdox::Exception const& ex)
    {
        // Any exception derived from Exception should
        // be caught and handled, and never make it here.
        clang::mrdox::reportUnhandledException(ex);
        return EXIT_FAILURE;
    }
    catch(std::exception const& ex)
    {
        // Any exception not derived from Exception which
        // makes it here must be reported and exit the program.
        clang::mrdox::reportUnhandledException(ex);
        return EXIT_FAILURE;
    }
}
