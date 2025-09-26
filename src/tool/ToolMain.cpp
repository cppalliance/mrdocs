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

#include "ToolArgs.hpp"
#include <lib/Support/Debug.hpp>
#include <lib/Support/Report.hpp>
#include <mrdocs/Support/Path.hpp>
#include <mrdocs/Version.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Host.h>
#include <cstdlib>
#include <ranges>

extern int main(int argc, char const** argv);

namespace clang::mrdocs {

extern
int
DoTestAction(char const** argv);

extern
Expected<void>
DoGenerateAction(
    std::string const& configPath,
    ReferenceDirectories const& dirs,
    char const** argv);


Expected<ReferenceDirectories>
getReferenceDirectories(std::string const& execPath)
{
    ReferenceDirectories dirs;
    dirs.mrdocsRoot = files::getParentDir(execPath, 2);
    llvm::SmallVector<char, 256> cwd;
    if (auto ec = llvm::sys::fs::current_path(cwd); ec)
    {
        return Unexpected(formatError("Unable to determine current working directory: {}", ec.message()));
    }
    dirs.cwd = std::string(cwd.data(), cwd.size());
    return dirs;
}

Expected<std::string>
getConfigPath(ReferenceDirectories const& dirs)
{
    std::string configPath;
    auto cmdLineFilenames = std::ranges::views::transform(
        toolArgs.cmdLineInputs, files::getFileName);
    if (!toolArgs.config.getValue().empty())
    {
        // From explicit --config argument
        configPath = toolArgs.config.getValue();
    }
    else if (auto const it = std::ranges::find(cmdLineFilenames, "mrdocs.yml");
             it != cmdLineFilenames.end())
    {
        // From implicit command line inputs
        configPath = *(it.base());
    }
    else if (files::exists("./mrdocs.yml"))
    {
        // From current directory
        configPath = "./mrdocs.yml";
    }
    else
    {
        return Unexpected(formatError("The config path is missing"));
    }
    configPath = files::makeAbsolute(configPath, dirs.cwd);
    return configPath;
}

int
mrdocs_main(int argc, char const** argv)
{
    // Enable stack traces
    llvm::EnablePrettyStackTrace();
    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
    llvm::setBugReportMsg("PLEASE submit a bug report to https://github.com/cppalliance/mrdocs/issues/ and include the crash backtrace.\n");

    // Set up addons directory
#ifdef __GNUC__
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wpedantic"
    // error: ISO C++ forbids taking address of function ‘::main’
#endif
    void* addressOfMain = reinterpret_cast<void*>(&main);
#ifdef __GNUC__
#    pragma GCC diagnostic pop
#endif
    std::string execPath = llvm::sys::fs::
        getMainExecutable(argv[0], addressOfMain);

    // Parse command line options
    llvm::cl::SetVersionPrinter([execPath](llvm::raw_ostream& os) {
        os << project_name << " version " << project_version_with_build << "\n";
        os << "Built with LLVM " << LLVM_VERSION_STRING << "\n";
        os << "Build SHA: " << project_version_build << "\n";
        os << "Target: " << llvm::sys::getDefaultTargetTriple() << "\n";
        os << "InstalledDir: " << files::getParentDir(execPath) << "\n";
    });

    toolArgs.hideForeignOptions();
    if (!llvm::cl::ParseCommandLineOptions(
        argc, argv, toolArgs.usageText))
    {
        return EXIT_FAILURE;
    }


    // Before `DoGenerateAction`, we use an error reporting level.
    // DoGenerateAction will set the level to whatever is specified in
    // the command line or the configuration file
    report::setMinimumLevel(report::Level::error);
    auto res = getReferenceDirectories(execPath);
    if (!res)
    {
        report::fatal("Failed to determine reference directories: {}", res.error().message());
        return EXIT_FAILURE;
    }
    auto dirs = *std::move(res);

    auto expConfigPath = getConfigPath(dirs);
    if (!expConfigPath)
    {
        report::fatal("Failed to determine config path: {}", expConfigPath.error().message());
        return EXIT_FAILURE;
    }
    auto configPath = *std::move(expConfigPath);

    // Generate
    if (auto exp = DoGenerateAction(configPath, dirs, argv); !exp)
    {
        report::error("Generating reference failed: {}", exp.error());
    }
    if (report::results.errorCount > 0 ||
        report::results.fatalCount > 0)
    {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

#ifdef _NDEBUG
static
void
reportUnhandledException(
    std::exception const& ex)
{
    namespace sys = llvm::sys;

    report::fatal("Unhandled exception: {}\n", ex.what());
    sys::PrintStackTrace(llvm::errs());
}
#endif

} // clang::mrdocs

int
main(int argc, char const** argv)
{
#ifndef _NDEBUG
    return clang::mrdocs::mrdocs_main(argc, argv);
#else
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
#endif
}
