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

#include <mrdocs/Config.hpp>
#include <llvm/Support/CommandLine.h>
#include <string>
#include <tool/PublicToolArgs.hpp>


namespace mrdocs {

/** Command line options and tool settings.
*/
class ToolArgs : public PublicToolArgs
{
    ToolArgs();

public:
    static ToolArgs instance_;

    char const*                 usageText;
    llvm::cl::extrahelp         extraHelp;

    /** Output directory for JSON Schema files.

        When set, the tool writes schema files describing
        the DOM and XML output formats, then exits without
        extracting any source files.
    */
    llvm::cl::opt<std::string>  schemas{
        "schemas",
        llvm::cl::desc("Write JSON Schema files to <dir> (default: .) and exit"),
        llvm::cl::value_desc("dir"),
        llvm::cl::ValueOptional,
        llvm::cl::cat(outputCat)};

    // Hide all options that don't belong to us
    void
    hideForeignOptions();
};

/** Command line arguments passed to the tool.

    This is a global variable because of how the
    LLVM command line interface is designed.
*/
constexpr static ToolArgs& toolArgs = ToolArgs::instance_;

} // mrdocs


#endif
