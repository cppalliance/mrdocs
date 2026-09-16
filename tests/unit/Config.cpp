//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Config.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <mrdocs/Support/Filesystem/Temp.hpp>
#include <test_suite/test_suite.hpp>
#include <fstream>
#include <string>

namespace mrdocs {

struct ConfigTest
{
    void
    testSourceRootDefaultsToConfigDir()
    {
        ScopedTempDirectory td("mrdocs-config");
        BOOST_TEST(td);
        // `addons` must name a directory that exists, and nothing reads
        // it here, so an empty one will do.
        std::string const addonsDir = files::appendPath(td.path(), "addons");
        BOOST_TEST(files::createDirectory(addonsDir).has_value());
        std::string const configPath =
            files::appendPath(td.path(), "mrdocs.yml");
        {
            std::ofstream os(configPath, std::ios::binary | std::ios::trunc);
            os << "addons: addons\n";
        }

        Config config;
        ReferenceDirectories const dirs;
        Expected<void> const loaded =
            Config::load_file(config, configPath, dirs);
        BOOST_TEST(loaded.has_value());
        BOOST_TEST(config.sourceRoot == files::makePosixStyle(td.path()));
    }

    void
    run()
    {
        testSourceRootDefaultsToConfigDir();
    }
};

TEST_SUITE(
    ConfigTest,
    "clang.mrdocs.Config");

} // mrdocs
