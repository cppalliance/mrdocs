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

extern
int
DoTestAction();

extern
Expected<void>
DoGenerateAction(
    std::string const& configPath,
    Config::Settings::ReferenceDirectories const& dirs,
    char const** argv);

void
print_version(llvm::raw_ostream& os)
{
    os << project_name
       << "\n    " << project_description
       << "\n    version: " << project_version
       << "\n    built with LLVM " << LLVM_VERSION_STRING
       << "\n";
}

Expected<std::pair<std::string, Config::Settings::ReferenceDirectories>>
getReferenceDirectories(std::string const& execPath)
{
    Config::Settings::ReferenceDirectories dirs;
    dirs.mrdocsRoot = files::getParentDir(execPath, 2);
    llvm::SmallVector<char, 256> cwd;
    if (auto ec = llvm::sys::fs::current_path(cwd); ec)
    {
        return Unexpected(formatError("Unable to determine current working directory: {}", ec.message()));
    }
    dirs.cwd = std::string(cwd.data(), cwd.size());
    std::string configPath;
    if (toolArgs.config.getValue() != "")
    {
        configPath = toolArgs.config.getValue();
    }
    else
    {
        llvm::cl::list<std::string>& inputs = toolArgs.inputs;
        for (auto& input: inputs)
        {
            if (files::getFileName(input) == "mrdocs.yml")
            {
                configPath = input;
                break;
            }
        }
    }
    if (configPath.empty())
    {
        if (files::exists("./mrdocs.yml"))
        {
            configPath = "./mrdocs.yml";
        }
    }
    if (configPath.empty())
    {
        return Unexpected(formatError("The config path is missing"));
    }
    configPath = files::makeAbsolute(configPath, dirs.cwd);
    dirs.configDir = files::getParentDir(configPath);
    return std::make_pair(configPath, dirs);
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

    // Apply report level
    report::setMinimumLevel(report::getLevel(
        toolArgs.report.getValue()));

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
    std::string execPath = llvm::sys::fs::getMainExecutable(argv[0], addressOfMain);

    auto res = getReferenceDirectories(execPath);
    if (!res)
    {
        report::fatal("Failed to determine reference directories: {}", res.error().message());
        return EXIT_FAILURE;
    }
    auto [configPath, dirs] = *res;

    // Generate
    auto exp = DoGenerateAction(configPath, dirs, argv);
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
