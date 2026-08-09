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

#ifndef MRDOCS_TEST_GOLDEN_TESTCLIARGS_HPP
#define MRDOCS_TEST_GOLDEN_TESTCLIARGS_HPP

#include "Action.hpp"
#include <string>
#include <vector>

namespace mrdocs {

/** The golden-test harness's own command-line options.

    The harness parses argv itself (like the mrdocs tool) instead of
    registering llvm::cl options, so it no longer duplicates the configuration
    options. Only the harness-specific options live here; configuration
    overrides (`--addons`, `--stdlib-includes`, ...) stay in argv and are
    applied by @ref Config::load.
*/
struct TestCliArgs
{
    /// What to do with each fixture (`--action=test|create|update`).
    Action action = Action::test;

    /// Write a `.bad.<ext>` file for each failure (`--bad`).
    bool bad = false;

    /// Rewrite expected files even when normalized contents match (`--force`).
    bool force = false;

    /// The harness reporting level (`--log-level`; also a config option).
    std::string logLevel = "info";

    /// The directories or files to test (positional arguments).
    std::vector<std::string> inputs;

    /// Whether `--help` was requested.
    bool showHelp = false;
};

/** Parse the harness's own options and test paths out of argv.

    Configuration options are recognized (so their value tokens are not
    mistaken for test paths) but left in argv for @ref Config::load to apply;
    unknown options are ignored.

    @param argc The argument count.
    @param argv The argument vector.
    @return The parsed harness options.
*/
TestCliArgs
parseTestCommandLine(int argc, char const** argv);

/// The parsed harness options, populated once at startup.
extern TestCliArgs testCliArgs;

} // mrdocs


#endif // MRDOCS_TEST_GOLDEN_TESTCLIARGS_HPP
