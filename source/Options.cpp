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

char const* Overview =
R"(Generate reference documentation, run tests against
a set of input vectors, or update a set of reference tests.)";

llvm::cl::OptionCategory Category("mrdox options");

llvm::cl::extrahelp ExtraHelp(
R"(Usage:

    mrdox .. ( compile-commands )

    mrdox .. --action ( "test" | "update" ) ( dir | file )...

Examples

    mrdox --action test friend.cpp

    mrdox --format adoc compile_commands.json
)");

llvm::cl::opt<Action> ToolAction(
    "action",
    llvm::cl::desc(R"(Which action should be performed)"),
    llvm::cl::init(Action::generate),
    llvm::cl::values(
        clEnumVal(test, "Compare output against expected"),
        clEnumVal(update, "Update all expected xml files"),
        clEnumVal(generate, "Generate reference documentation")),
    llvm::cl::cat(Category));

// Test options

llvm::cl::opt<bool> badOption(
    "bad",
    llvm::cl::desc("Write a .bad.xml file for each test failure"),
    llvm::cl::init(true),
    llvm::cl::cat(Category));

// Generate options

llvm::cl::opt<std::string> FormatType(
    "format",
    llvm::cl::desc("Format for outputted docs (\"adoc\" or \"xml\")."),
    llvm::cl::init("adoc"),
    llvm::cl::cat(Category));

// Common options

llvm::cl::opt<bool> IgnoreMappingFailures(
    "ignore-map-errors",
    llvm::cl::desc("Continue if files are not mapped correctly."),
    llvm::cl::init(true),
    llvm::cl::cat(Category));

llvm::cl::opt<std::string> ConfigPath(
    "config",
    llvm::cl::desc(R"(The config filename relative to the repository root)"),
    llvm::cl::init("mrdox.yml"),
    llvm::cl::cat(Category));

llvm::cl::opt<std::string> OutputPath(
    "output",
    llvm::cl::desc("Directory or file for generating output."),
    llvm::cl::init("."),
    llvm::cl::cat(Category));

llvm::cl::list<std::string> InputPaths(
    "inputs",
    llvm::cl::Sink,
    llvm::cl::desc("The path to the compilation database, or one or more .cpp files to test."),
    llvm::cl::cat(Category));

llvm::cl::opt<std::string>   PluginsPath(
    "plugins-path",
    llvm::cl::desc("The plugins directory"),
    llvm::cl::cat(Category)
    );


} // mrdox
} // clang
