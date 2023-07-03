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

//------------------------------------------------

// This is a tool for generating reference documentation from C++ source code.
// It runs a LibTooling FrontendAction on source files, mapping each declaration
// in those files to its USR and serializing relevant information into LLVM
// bitcode. It then runs a pass over the collected declaration information,
// reducing by USR. Finally, it hands the reduced information off to a generator,
// which does the final parsing from the intermediate representation to the
// desired output format.
//
// The tool comes with these builtin generators:
//
//  XML
//  Asciidoc
//  Bitstream
//
// Furthermore, additional generators can be implemented as dynamically
// loaded library "plugins" discovered at runtime. These generators can
// be implemented without including LLVM headers or linking to LLVM
// libraries.

//------------------------------------------------

#include "ToolArgs.hpp"
#include "Support/Debug.hpp"
#include <mrdox/Support/Path.hpp>
#include <mrdox/Support/Report.hpp>
#include <mrdox/Version.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Process.h>
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
    using Process = llvm::sys::Process;

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

    // Set addons dir
    std::string addonsDir;
    if(! toolArgs.addonsDir.getValue().empty())
    {
        // from command line
        auto absPath = files::makeAbsolute(
            toolArgs.addonsDir.getValue());
        if(! absPath)
            reportError(absPath.error(), "set the addons directory");
        addonsDir = files::makeDirsy(files::normalizePath(*absPath));
        if(auto err = files::requireDirectory(addonsDir))
        {
            reportError(err, "set the addons directory");
            return EXIT_FAILURE;
        }
        toolArgs.addonsDir.getValue() = addonsDir;
    }
    else
    {
        // check process working directory
        addonsDir = fs::getMainExecutable(argv[0], reinterpret_cast<void*>(&main));
        if(addonsDir.empty())
        {
            reportError(
                "Could locate the executable because "
                "fs::getMainExecutable failed.");
            return EXIT_FAILURE;
        }
        addonsDir = files::makeDirsy(files::appendPath(
            files::getParentDir(addonsDir), "addons"));
        if(! files::requireDirectory(addonsDir).failed())
        {
            // from directory containing the process executable
            toolArgs.addonsDir.getValue() = addonsDir;
        }
        else
        {
            auto addonsEnvVar = Process::GetEnv("MRDOX_ADDONS_DIR");
            if(! addonsEnvVar.has_value())
            {
                reportError(
                    "Could not locate the addons directory because "
                    "the MRDOX_ADDONS_DIR environment variable is not set, "
                    "no addons location was specified on the command line, "
                    "and no addons directory exists in the same directory as "
                    "the executable.");
                return EXIT_FAILURE;
            }
            // from environment variable
            addonsDir = files::makeDirsy(files::normalizePath(*addonsEnvVar));
            if(auto err = files::requireAbsolute(addonsDir))
                reportError(err, "set the addons directory");
            if(auto err = files::requireDirectory(addonsDir))
            {
                reportError(err, "set the addons directory");
                return EXIT_FAILURE;
            }
            toolArgs.addonsDir.getValue() = addonsDir;
        }
    }

    // Generate
    if(toolArgs.toolAction == Action::generate)
    {
        auto err = DoGenerateAction();
        if(err)
        {
            reportError(err, "generate reference documentation");
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }

    // Test
    return DoTestAction();
}

namespace lua {
extern void lua_main();
}

} // mrdox
} // clang

int main(int argc, char const** argv)
{
//clang::mrdox::lua::lua_main();
    try
    {
        return clang::mrdox::mrdox_main(argc, argv);
    }
    catch(clang::mrdox::Exception const& ex)
    {
        // Any exception derived from Exception should
        // be caught and handled, and never make it here.
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
