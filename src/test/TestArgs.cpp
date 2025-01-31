//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "TestArgs.hpp"
#include <vector>

namespace clang::mrdocs {

TestArgs TestArgs::instance_;

//------------------------------------------------

TestArgs::
TestArgs()
    : PublicToolArgs()

    , usageText("MrDocs Test Program")
    , extraHelp(
R"(
EXAMPLES:
    mrdocs-test .. ( compile-commands )
    mrdocs-test .. --action ( "test" | "create" | "update" ) ( dir | file )...
    mrdocs-test --action test friend.cpp
)")

//
// Test options
//
, action(
    "action",
    llvm::cl::desc(R"(Which action should be performed:)"),
    llvm::cl::init(test),
    llvm::cl::values(
        clEnumVal(test, "Compare output against expected."),
        clEnumVal(create, "Create missing expected documentation files."),
        clEnumVal(update, "Update all expected documentation files.")))

, badOption(
    "bad",
    llvm::cl::desc("Write a .bad.xml file for each test failure."),
    llvm::cl::init(true))

, unitOption(
    "unit",
    llvm::cl::desc("Run all or selected unit test suites."),
    llvm::cl::init(true))
{
}

void
TestArgs::
hideForeignOptions() const
{
    // VFALCO When adding an option, it must
    // also be added to this list or else it
    // will stay hidden.

    std::vector<llvm::cl::Option const*> ours({
        &action,
        &badOption,
        &unitOption
    });

    // Really hide the clang/llvm default
    // options which we didn't ask for.
    for (auto optionMap = llvm::cl::getRegisteredOptions();
         auto& opt : optionMap)
    {
        if (std::ranges::find(ours, opt.getValue()) != ours.end())
        {
            opt.getValue()->setHiddenFlag(llvm::cl::NotHidden);
        } else
        {
            opt.getValue()->setHiddenFlag(llvm::cl::ReallyHidden);
        }
    }
}

} // clang::mrdocs
