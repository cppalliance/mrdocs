//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "Addons.hpp"
#include "ToolArgs.hpp"
#include "lib/Support/Debug.hpp"
#include "lib/Support/Error.hpp"
#include <mrdocs/Support/Path.hpp>
#include <mrdocs/Version.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>
#include <cstdlib>

extern int main(int argc, char const** argv);

namespace clang {
namespace mrdocs {

extern int DoTestAction();
extern Expected<void> DoGenerateAction();

void
print_version(llvm::raw_ostream& os)
{
    os << project_name
       << "\n    " << project_description
       << "\n    version: " << project_version
       << "\n    built with LLVM " << LLVM_VERSION_STRING
       << "\n";
}

int
mrdocs_main(int argc, char const** argv)
{
    // Enable stack traces
    llvm::EnablePrettyStackTrace();
    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
    llvm::setBugReportMsg("PLEASE submit a bug report to https://github.com/cppalliance/mrdocs/issues/ and include the crash backtrace.\n");

    // Parse command line options
    llvm::cl::SetVersionPrinter(&print_version);
    toolArgs.hideForeignOptions();
    if (!llvm::cl::ParseCommandLineOptions(
            argc, argv, toolArgs.usageText))
    {
        return EXIT_FAILURE;
    }

    // Apply reportLevel
    report::setMinimumLevel(report::getLevel(
        toolArgs.reportLevel.getValue()));

    // Set up addons directory
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
// error: ISO C++ forbids taking address of function ‘::main’
#endif
    void* addressOfMain = reinterpret_cast<void*>(&main);
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
    auto exp = setupAddonsDir(
            toolArgs.addonsDir,argv[0], addressOfMain);
    if (!exp)
    {
        report::error(
            "{}: \"{}\"\n"
            "Could not locate the addons directory because "
            "the MRDOCS_ADDONS_DIR environment variable is not set, "
            "no valid addons location was specified on the command line, "
            "and no addons directory exists in the same directory as "
            "the executable.",
            exp.error(), toolArgs.addonsDir);
        return EXIT_FAILURE;
    }

    // Generate
    exp = DoGenerateAction();
    if (!exp)
    {
        report::error("Generating reference failed: {}", exp.error().message());
    }
    if (report::results.errorCount > 0 ||
        report::results.fatalCount > 0)
    {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

static
void
reportUnhandledException(
    std::exception const& ex)
{
    namespace sys = llvm::sys;

    report::fatal("Unhandled exception: {}\n", ex.what());
    sys::PrintStackTrace(llvm::errs());
}

} // mrdocs
} // clang

int
main(int argc, char const** argv)
{
    try
    {
        return clang::mrdocs::mrdocs_main(argc, argv);
    }
    catch(clang::mrdocs::Exception const& ex)
    {
        // Thrown Exception should never get here.
        clang::mrdocs::reportUnhandledException(ex);
    }
    catch(std::exception const& ex)
    {
        clang::mrdocs::reportUnhandledException(ex);
    }
    return EXIT_FAILURE;
}
