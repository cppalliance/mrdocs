//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_TEST_OPTIONS_HPP
#define MRDOX_TEST_OPTIONS_HPP

#include <llvm/Support/CommandLine.h>

namespace clang {
namespace mrdox {

enum Action : int
{
    test,
    refresh
};

class Options
{
    llvm::cl::extrahelp ExtraHelp;
    llvm::cl::OptionCategory TestCategory;

public:
    char const* Overview;
    llvm::cl::opt<Action> TestAction;
    llvm::cl::opt<bool> BadOption;
    llvm::cl::list<std::string> InputPaths;

    Options();
};


} // mrdox
} // clang

#endif
