//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Config.hpp>
#include <mrdocs/Session.hpp>
#include <mrdocs/Support/Reporter.hpp>
#include <test_suite/test_suite.hpp>

namespace mrdocs {

struct SessionTest
{
    // Two sessions in one process keep their own level, their own
    // `warn-as-error` and their own totals.
    void
    testIndependentSessions()
    {
        Config quiet;
        quiet.logLevel = ConfigSchema::LogLevel::Fatal;
        quiet.warnAsError = true;
        Config loud;
        loud.logLevel = ConfigSchema::LogLevel::Error;
        Expected<Session> loadedFirst = Session::create(quiet);
        Expected<Session> loadedSecond = Session::create(loud);
        BOOST_TEST(loadedFirst.has_value());
        BOOST_TEST(loadedSecond.has_value());
        if (!loadedFirst || !loadedSecond)
        {
            return;
        }
        Session& first = *loadedFirst;
        Session& second = *loadedSecond;
        BOOST_TEST(first.log().getMinimumLevel() == report::Level::fatal);
        BOOST_TEST(second.log().getMinimumLevel() == report::Level::error);
        BOOST_TEST(first.log().warnAsError());
        BOOST_TEST(!second.log().warnAsError());

        // A warning is an error under `warn-as-error`, and only there
        first.log().warn("counted by the first session only");
        BOOST_TEST(first.log().results().warnCount == 0u);
        BOOST_TEST(first.log().results().errorCount == 1u);
        BOOST_TEST(second.log().results().warnCount == 0u);
        BOOST_TEST(second.log().results().errorCount == 0u);
        second.log().warn("counted by the second session only");
        BOOST_TEST(second.log().results().warnCount == 1u);
        BOOST_TEST(second.log().results().errorCount == 0u);
        BOOST_TEST(first.log().results().warnCount == 0u);

        second.log().setMinimumLevel(report::Level::fatal);
        second.log().info("counted by the second session only");
        BOOST_TEST(second.log().results().infoCount == 1u);
        BOOST_TEST(first.log().results().infoCount == 0u);
        BOOST_TEST(first.log().getMinimumLevel() == report::Level::fatal);
    }

    void
    run()
    {
        testIndependentSessions();
    }
};

TEST_SUITE(
    SessionTest,
    "clang.mrdocs.Session");

} // mrdocs
