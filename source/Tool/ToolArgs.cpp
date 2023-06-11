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

//------------------------------------------------

ToolArgs::
ToolArgs()
    : genCat("Generation Options")
    , testCat("Test Options")
    , usageText(
R"( Generate C++ reference documentation
)")
    , extraHelp(
R"(
USAGE:
    mrdox .. ( compile-commands )
    mrdox .. --action ( "test" | "update" ) ( dir | file )...

EXAMPLES:
    mrdox --action test friend.cpp
    mrdox --format adoc compile_commands.json
)")

//
// Common options
//

, toolAction(
    "action",
    llvm::cl::desc(R"(Which action should be performed)"),
    llvm::cl::init(Action::generate),
    llvm::cl::values(
        clEnumVal(test, "Compare output against expected"),
        clEnumVal(update, "Update all expected xml files"),
        clEnumVal(generate, "Generate reference documentation")))

, configPath(
    "config",
    llvm::cl::desc(R"(The config filename relative to the repository root)"))

, outputPath(
    "output",
    llvm::cl::desc("Directory or file for generating output."),
    llvm::cl::init("."))

, inputPaths(
    "inputs",
    llvm::cl::Sink,
    llvm::cl::desc("The path to the compilation database, or one or more .cpp files to test."))

//
// Generate options
//

, formatType(
    "format",
    llvm::cl::desc("Format for outputted docs (\"adoc\" or \"xml\")."),
    llvm::cl::init("adoc"),
    llvm::cl::cat(genCat))

, ignoreMappingFailures(
    "ignore-map-errors",
    llvm::cl::desc("Continue if files are not mapped correctly."),
    llvm::cl::init(true),
    llvm::cl::cat(genCat))

//
// Test options
//

, badOption(
    "bad",
    llvm::cl::desc("Write a .bad.xml file for each test failure"),
    llvm::cl::init(true),
    llvm::cl::cat(testCat))
{
}

ToolArgs ToolArgs::instance_;

void
ToolArgs::
hideForeignOptions()
{
    // VFALCO When adding an option, it must
    // also be added to this list or else it
    // will stay hidden.

    std::vector<llvm::cl::Option const*> ours({
        &toolAction,
        &formatType,
        &configPath,
        &outputPath,
        std::addressof(inputPaths),
        &ignoreMappingFailures,
        &badOption
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
