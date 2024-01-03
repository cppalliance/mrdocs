//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_TOOL_TOOLARGS_HPP
#define MRDOCS_TOOL_TOOLARGS_HPP

#include <llvm/Support/CommandLine.h>
#include <string>

namespace clang {
namespace mrdocs {

/** Command line options and tool settings.
*/
class ToolArgs
{
    ToolArgs();

    llvm::cl::OptionCategory    commonCat;

public:
    static ToolArgs instance_;

    char const*                 usageText;
    llvm::cl::extrahelp         extraHelp;

    llvm::cl::opt<std::string>  addonsDir;
    llvm::cl::opt<unsigned>     reportLevel;
    llvm::cl::opt<unsigned>     concurrency;

    llvm::cl::opt<std::string>  configPath;
    llvm::cl::opt<std::string>  outputPath;
    llvm::cl::opt<bool>         ignoreMappingFailures;
    llvm::cl::list<std::string> inputPaths;

    llvm::cl::opt<bool>         generateCompilationDatabase;
    llvm::cl::opt<std::string>  cmakePath;
    llvm::cl::opt<std::string>  cmakeListsPath;

    // Hide all options which don't belong to us
    void hideForeignOptions();
};

/** Command line arguments passed to the tool.

    This is a global variable because of how the
    LLVM command line interface is designed.
*/
constexpr static ToolArgs& toolArgs = ToolArgs::instance_;

} // mrdocs
} // clang

#endif
