//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_TOOL_TOOLARGS_HPP
#define MRDOX_TOOL_TOOLARGS_HPP

#include <llvm/Support/CommandLine.h>
#include <string>

namespace clang {
namespace mrdox {

enum Action : int
{
    test,
    update,
    generate
};

/** Command line options and tool settings.
*/
class ToolArgs
{
    ToolArgs();

    llvm::cl::OptionCategory    commonCat;
    llvm::cl::OptionCategory    generateCat;
    llvm::cl::OptionCategory    testCat;

public:
    static ToolArgs instance_;

    char const*                 usageText;
    llvm::cl::extrahelp         extraHelp;

    // Common options
    llvm::cl::opt<Action>       toolAction;
    llvm::cl::opt<std::string>  addonsDir;
    llvm::cl::opt<std::string>  configPath;
    llvm::cl::opt<std::string>  outputPath;
    llvm::cl::list<std::string> inputPaths;

    // Generate options
    llvm::cl::opt<std::string>  formatType;
    llvm::cl::opt<bool>         ignoreMappingFailures;

    // Test options
    llvm::cl::opt<bool>         badOption;

    // Hide all options which don't belong to us
    void hideForeignOptions();
};

/** Command line arguments passed to the tool.

    This is a global variable because of how the
    LLVM command line interface is designed.
*/
constexpr static ToolArgs& toolArgs = ToolArgs::instance_;

} // mrdox
} // clang

#endif
