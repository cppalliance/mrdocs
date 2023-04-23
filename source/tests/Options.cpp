//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "Options.hpp"

namespace clang {
namespace mrdox {

Options::
Options()
: ExtraHelp(R"(
Usage

    mrdox-test options... ( dir | file )...

Examples

    mrdox-test friend.cpp
)")

, TestCategory("mrdox-test options")

, Overview(
R"(Test the output of MrDox against a set of input vectors.)")

, badOption(
    "bad",
    llvm::cl::desc("Write a .bad.xml file for each test failure"),
    llvm::cl::init(true),
    llvm::cl::cat(TestCategory))

, adocOption(
    "adoc",
    llvm::cl::desc("Write the corresponding Asciidoc (adoc) file for each input test file"),
    llvm::cl::init(false),
    llvm::cl::cat(TestCategory))

, TestAction(
    "action",
    llvm::cl::desc(R"(Which action should be performed)"),
    llvm::cl::init(Action::test),
    llvm::cl::values(
        clEnumVal(test, "Compare output against expected"),
        clEnumVal(refresh, "Update all expected xml files")),
    llvm::cl::cat(TestCategory))

, InputPaths(
    "inputs",
    llvm::cl::Sink,
    llvm::cl::desc("The list of directories and/or .cpp files to test"),
    llvm::cl::cat(TestCategory))
{
}

} // mrdox
} // clang
