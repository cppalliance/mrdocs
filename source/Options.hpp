//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_TOOL_OPTIONS_HPP
#define MRDOX_TOOL_OPTIONS_HPP

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

extern char const* Overview;
extern llvm::cl::OptionCategory     Category;
extern llvm::cl::extrahelp          ExtraHelp;
extern llvm::cl::opt<Action>        ToolAction;

// Test options
extern llvm::cl::opt<bool> badOption;
extern llvm::cl::opt<bool> adocOption;

// Generate options
extern llvm::cl::opt<std::string> FormatType;

// Common options
extern llvm::cl::opt<bool>          IgnoreMappingFailures;
extern llvm::cl::opt<std::string>   ConfigPath;
extern llvm::cl::opt<std::string>   OutputPath;
extern llvm::cl::list<std::string>  InputPaths;

} // mrdox
} // clang

#endif
