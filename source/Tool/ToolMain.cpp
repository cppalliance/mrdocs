//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

//------------------------------------------------

// This is a tool for generating reference documentation from C++ source code.
// It runs a LibTooling FrontendAction on source files, mapping each declaration
// in those files to its USR and serializing relevant information into LLVM
// bitcode. It then runs a pass over the collected declaration information,
// reducing by USR. Finally, it hands the reduced information off to a generator,
// which does the final parsing from the intermediate representation to the
// desired output format.
//
// The tool comes with these builtin generators:
//
//  XML
//  Asciidoc
//  Bitstream
//
// Furthermore, additional generators can be implemented as dynamically
// loaded library "plugins" discovered at runtime. These generators can
// be implemented without including LLVM headers or linking to LLVM
// libraries.

//------------------------------------------------

#include "ToolArgs.hpp"
#include "Support/Debug.hpp"
#include <mrdox/Support/Report.hpp>
#include <mrdox/Version.hpp>
#include <llvm/Support/Signals.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>
#include <stdlib.h>

namespace clang {
namespace mrdox {

extern Error DoGenerateAction();
extern int DoTestAction();

inline void print_version(llvm::raw_ostream& os)
{
    os << project_name
       << "\n    " << project_description
       << "\n    version: " << project_version
       << "\n    built with LLVM " << LLVM_VERSION_STRING;
}

} // mrdox
} // clang

int main(int argc, char const** argv)
{
    using namespace clang::mrdox;

    // VFALCO this heap checking is too strong for
    // a clang tool's model of what is actually a leak.
    // clang::mrdox::debugEnableHeapChecking();

    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
    llvm::cl::SetVersionPrinter(&print_version);

    toolArgs.hideForeignOptions();
    if(! llvm::cl::ParseCommandLineOptions(
            argc, argv, toolArgs.usageText))
        return EXIT_FAILURE;

    // Generate
    if(clang::mrdox::toolArgs.toolAction == Action::generate)
    {
        auto err = DoGenerateAction();
        if(! err)
            return EXIT_SUCCESS;

        reportError(err, "generate reference documentation");
        return EXIT_FAILURE;
    }

    // Test
    return DoTestAction();
}
