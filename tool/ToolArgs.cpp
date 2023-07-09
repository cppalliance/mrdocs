//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "ToolArgs.hpp"
#include <cstddef>
#include <vector>

namespace clang {
namespace mrdox {

ToolArgs ToolArgs::instance_;

//------------------------------------------------

ToolArgs::
ToolArgs()
    : commonCat("COMMON")

    , usageText(
R"( Generate C++ reference documentation
)")

    , extraHelp(
R"(
ADDONS:
    The location of the addons directory is determined in this order:

    1. The --addons command line argument if present, or
    2. The directory containing the mrdox tool executable, otherwise
    3. The environment variable MRDOX_ADDONS_DIR if set.

EXAMPLES:
    mrdox .. ( compile-commands )
    mrdox --format adoc compile_commands.json
)")

//
// Common options
//

, addonsDir(
    "addons",
    llvm::cl::desc("The path to the addons directory."),
    llvm::cl::cat(commonCat))

, reportLevel(
    "report",
    llvm::cl::desc("The minimum reporting level (0 to 4)."),
    llvm::cl::init(1),
    llvm::cl::cat(commonCat))

, concurrency(
    "concurrency",
    llvm::cl::desc("The desired level of concurrency (0 for hardware-suggested)."),
    llvm::cl::init(0),
    llvm::cl::cat(commonCat))

//
// Tool options
//

, configPath(
    "config",
    llvm::cl::desc(R"(The config filename relative to the repository root.)"))

, outputPath(
    "output",
    llvm::cl::desc("Directory or file for generating output."),
    llvm::cl::init("."))

, formatType(
    "format",
    llvm::cl::desc("Format for the emitted documentation (\"adoc\" or \"xml\")."),
    llvm::cl::init("adoc"))

, ignoreMappingFailures(
    "ignore-map-errors",
    llvm::cl::desc("Continue if files are not mapped correctly."),
    llvm::cl::init(true))

, inputPaths(
    "inputs",
    llvm::cl::Sink,
    llvm::cl::desc("The path to the compilation database."))
{
}

void
ToolArgs::
hideForeignOptions()
{
    // VFALCO When adding an option, it must
    // also be added to this list or else it
    // will stay hidden.

    std::vector<llvm::cl::Option const*> ours({
        &addonsDir,
        &configPath,
        &outputPath,
        std::addressof(inputPaths),
        &formatType,
        &ignoreMappingFailures,
    });

    // Really hide the clang/llvm default
    // options which we didn't ask for.
    auto optionMap = llvm::cl::getRegisteredOptions();
    for(auto& opt : optionMap)
    {
        if(std::find(ours.begin(), ours.end(), opt.getValue()) != ours.end())
            opt.getValue()->setHiddenFlag(llvm::cl::NotHidden);
        else
            opt.getValue()->setHiddenFlag(llvm::cl::ReallyHidden);
    }
}

} // mrdox
} // clang
