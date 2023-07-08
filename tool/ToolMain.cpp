//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "Tool/Addons.hpp"
#include "Tool/ToolArgs.hpp"
#include "Support/Debug.hpp"
#include <mrdox/Support/Error.hpp>
#include <mrdox/Support/Path.hpp>
#include <mrdox/Version.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>
#include <stdlib.h>

extern int main(int argc, char const** argv);

namespace clang {
namespace mrdox {

extern int DoTestAction();
extern Error DoGenerateAction();

void
print_version(llvm::raw_ostream& os)
{
    os << project_name
       << "\n    " << project_description
       << "\n    version: " << project_version
       << "\n    built with LLVM " << LLVM_VERSION_STRING;
}

int mrdox_main(int argc, char const** argv)
{
    namespace fs = llvm::sys::fs;

    // VFALCO this heap checking is too strong for
    // a clang tool's model of what is actually a leak.
    // debugEnableHeapChecking();

    llvm::EnablePrettyStackTrace();
    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
    llvm::cl::SetVersionPrinter(&print_version);

    toolArgs.hideForeignOptions();
    if(! llvm::cl::ParseCommandLineOptions(
            argc, argv, toolArgs.usageText))
        return EXIT_FAILURE;

    // Apply reportLevel
    report::setMinimumLevel(toolArgs.reportLevel.getValue());

    if(! setupAddonsDir(toolArgs.addonsDir, argv[0],
            reinterpret_cast<void*>(&main)))
        return EXIT_FAILURE;

    // Generate
    auto err = DoGenerateAction();
    if(err)
    {
        reportError(err, "generate reference documentation");
        return EXIT_FAILURE;
    }
    if( report::results.errorCount > 0 ||
        report::results.fatalCount > 0)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

} // mrdox
} // clang

int main(int argc, char const** argv)
{
    try
    {
        return clang::mrdox::mrdox_main(argc, argv);
    }
    catch(clang::mrdox::Exception const& ex)
    {
        // Any exception derived from Exception should
        // be caught and handled, and never make it here.
        clang::mrdox::reportUnhandledException(ex);
        MRDOX_UNREACHABLE();
    }
    catch(std::exception const& ex)
    {
        // Any exception not derived from Exception which
        // makes it here must be reported and terminate the
        // process immediately.
        clang::mrdox::reportUnhandledException(ex);
        return EXIT_FAILURE;
    }
}
