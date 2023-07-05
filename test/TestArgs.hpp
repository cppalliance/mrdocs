//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_TEST_TESTARGS_HPP
#define MRDOX_TEST_TESTARGS_HPP

#include <llvm/Support/CommandLine.h>
#include <string>

namespace clang {
namespace mrdox {

enum Action : int
{
    test,
    update,
};

/** Command line options and test settings.
*/
class TestArgs
{
    TestArgs();

    llvm::cl::OptionCategory    commonCat;

public:
    static TestArgs instance_;

    char const*                 usageText;
    llvm::cl::extrahelp         extraHelp;

    // Common options
    llvm::cl::opt<Action>       action;
    llvm::cl::opt<std::string>  addonsDir;
    llvm::cl::list<std::string> inputPaths;

    // Test options
    llvm::cl::opt<bool>         badOption;
    llvm::cl::opt<bool>         unitOption;

    // Hide all options which don't belong to us
    void hideForeignOptions();
};

/** Command line arguments passed to the tool.

    This is a global variable because of how the
    LLVM command line interface is designed.
*/
constexpr static TestArgs& testArgs = TestArgs::instance_;

} // mrdox
} // clang

#endif
