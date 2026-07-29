//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_TEST_GOLDEN_TESTARGS_HPP
#define MRDOCS_TEST_GOLDEN_TESTARGS_HPP

#include "Action.hpp"
#include <llvm/Support/CommandLine.h>
#include <string>
#include <tool/PublicToolArgs.hpp>


namespace mrdocs {

/** Command line options and test settings.
*/
class TestArgs : public PublicToolArgs
{
    TestArgs();

public:
    static TestArgs instance_;

    char const*                 usageText;
    llvm::cl::extrahelp         extraHelp;

    // Test options
    llvm::cl::opt<Action>       action;
    llvm::cl::opt<bool>         badOption;
    llvm::cl::opt<bool>         forceOption;
    llvm::cl::opt<bool>         unitOption;

    // Hide all options that don't belong to us
    void hideForeignOptions() const;
};

/** Command line arguments passed to the tool.

    This is a global variable because of how the
    LLVM command line interface is designed.
*/
constexpr static TestArgs& testArgs = TestArgs::instance_;

} // mrdocs


#endif // MRDOCS_TEST_GOLDEN_TESTARGS_HPP
