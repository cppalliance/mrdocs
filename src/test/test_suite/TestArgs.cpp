//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "TestArgs.hpp"
#include <fmt/format.h>
#include <cstddef>
#include <vector>

namespace clang {
namespace mrdox {

TestArgs TestArgs::instance_;

//------------------------------------------------

TestArgs::
TestArgs()
    : commonCat("COMMON")

    , usageText(
R"(MrDox Test Program
)")

    , extraHelp(
R"(
EXAMPLES:
    mrdox-test .. ( compile-commands )
    mrdox-test .. --action ( "test" | "create" | "update" ) ( dir | file )...
    mrdox-test --action test friend.cpp
)")

//
// Common options
//

, reportLevel(
    "report",
    llvm::cl::desc("The minimum reporting level (0 to 4)."),
    llvm::cl::init(2),
    llvm::cl::cat(commonCat))

//
// Test options
//

, action(
    "action",
    llvm::cl::desc(R"(Which action should be performed:)"),
    llvm::cl::init(test),
    llvm::cl::values(
        clEnumVal(test, "Compare output against expected."),
        clEnumVal(create, "Create missing expected xml files."),
        clEnumVal(update, "Update all expected xml files.")),
    llvm::cl::cat(commonCat))

, badOption(
    "bad",
    llvm::cl::desc("Write a .bad.xml file for each test failure."),
    llvm::cl::init(true))

, unitOption(
    "unit",
    llvm::cl::desc("Run all or selected unit test suites."),
    llvm::cl::init(true))

, inputPaths(
    "inputs",
    llvm::cl::Sink,
    llvm::cl::desc("A list of directories and/or .cpp files to test."),
    llvm::cl::cat(commonCat))
{
}

void
TestArgs::
hideForeignOptions()
{
    // VFALCO When adding an option, it must
    // also be added to this list or else it
    // will stay hidden.

    std::vector<llvm::cl::Option const*> ours({
        &action,
        std::addressof(inputPaths),
        &badOption,
        &unitOption
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
