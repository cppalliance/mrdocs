//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "lib/ConfigImpl.hpp"
#include "lib/MrDocsCompilationDatabase.hpp"
#include "lib/SingleFileDB.hpp"
#include <mrdocs/Support/Algorithm.hpp>
#include <mrdocs/Support/Path.hpp>
#include <mrdocs/Support/ThreadPool.hpp>
#include <test_suite/test_suite.hpp>

namespace clang {
namespace mrdocs {

namespace {
struct TestConfigImpl final : Config
{
    Settings settings_;

    ThreadPool&
        threadPool() const noexcept override
    {
        static ThreadPool dummyThreadPool_;
        return dummyThreadPool_;
    }

    dom::Object const& object() const override
    {
        static dom::Object dummyObject;
        return dummyObject;

    }

    Settings const& settings() const noexcept override
    {
        return settings_;
    }
};
}

struct MrDocsCompilationDatabase_test
{
    auto adjustCompileCommand(
        std::vector<std::string> commandLine,
        std::shared_ptr<Config const> config = std::make_shared<TestConfigImpl>()) const
    {
        tooling::CompileCommand cc;
        cc.Directory = ".";
        cc.Filename = "test.cpp"; // file does not exist
        cc.CommandLine = std::move(commandLine);
        cc.CommandLine.push_back(cc.Filename);
        cc.Heuristic = "unit test";

        // Create an adjusted MrDocsDatabase
        std::unordered_map<std::string, std::vector<std::string>> defaultIncludePaths;
        MrDocsCompilationDatabase compilations(
            "", SingleFileDB(std::move(cc)), config, defaultIncludePaths);
        return compilations.getAllCompileCommands().front().CommandLine;
    }

    static bool has(std::vector<std::string> const& commandLine, std::string_view flag)
    {
        return mrdocs::contains(commandLine, flag);
    }
    static bool has(std::vector<std::string> const& commandLine, std::initializer_list<std::string_view> flags)
    {
        MRDOCS_ASSERT(flags.size() > 0);
        return std::search(commandLine.begin(), commandLine.end(), flags.begin(), flags.end()) != commandLine.end();
    }

    void testClang()
    {
        std::string const programName = "clang";

        {
            auto adjusted = adjustCompileCommand({ programName });
            BOOST_TEST(has(adjusted, "-std=c++23"));
        }
        {
            auto adjusted = adjustCompileCommand({ programName, "-std=c++11" });
            BOOST_TEST(has(adjusted, "-std=c++11"));
            BOOST_TEST_NOT(has(adjusted, "-std=c++23"));
        }
        {
            auto adjusted = adjustCompileCommand({ programName, "--std=c++11" });
            BOOST_TEST(has(adjusted, "--std=c++11"));
            BOOST_TEST_NOT(has(adjusted, "-std=c++23"));
        }

        {
            std::shared_ptr<TestConfigImpl> config = std::make_shared<TestConfigImpl>();
            config->settings_.defines = { "FOO", "BAR=1" };

            auto adjusted = adjustCompileCommand({ programName, "-DBAZ=2" }, config);
            BOOST_TEST(has(adjusted, "-D__MRDOCS__"));
            BOOST_TEST(has(adjusted, "-DFOO"));
            BOOST_TEST(has(adjusted, "-DBAR=1"));
            BOOST_TEST(has(adjusted, "-DBAZ=2"));
        }

        {
            std::shared_ptr<TestConfigImpl> const config = std::make_shared<TestConfigImpl>();
            config->settings_.useSystemStdlib = false;
            config->settings_.stdlibIncludes.push_back("stdlib-path");

            auto adjusted = adjustCompileCommand({ programName }, config);
            BOOST_TEST(has(adjusted, "-nostdinc++"));
            BOOST_TEST(has(adjusted, { "-isystem", "stdlib-path" }));
        }

        {
            std::shared_ptr<TestConfigImpl> config = std::make_shared<TestConfigImpl>();
            config->settings_.useSystemLibc = false;
            config->settings_.libcIncludes.push_back("libc-path");

            auto adjusted = adjustCompileCommand({ programName }, config);
            BOOST_TEST(has(adjusted, "-nostdinc"));
            BOOST_TEST(has(adjusted, { "-isystem", "libc-path" }));
        }

        {
            std::shared_ptr<TestConfigImpl> config = std::make_shared<TestConfigImpl>();
            config->settings_.systemIncludes.push_back("system-path");

            auto adjusted = adjustCompileCommand({ programName }, config);
            BOOST_TEST(has(adjusted, { "-isystem", "system-path" }));
        }
    }

    void testClangCL()
    {
        std::string const programName = "clang-cl";

        {
            auto adjusted = adjustCompileCommand({ programName });
            BOOST_TEST(has(adjusted, "-std=c++23"));
        }
        {
            auto adjusted = adjustCompileCommand({ programName, "-std:c++11" });
            BOOST_TEST(has(adjusted, "-std:c++11"));
            BOOST_TEST_NOT(has(adjusted, "-std=c++23"));
        }
        {
            auto adjusted = adjustCompileCommand({ programName, "/std:c++11" });
            BOOST_TEST(has(adjusted, "/std:c++11"));
            BOOST_TEST_NOT(has(adjusted, "-std=c++23"));
        }

        {
            std::shared_ptr<TestConfigImpl> config = std::make_shared<TestConfigImpl>();
            config->settings_.defines = { "FOO", "BAR=1" };

            auto adjusted = adjustCompileCommand({ programName, "-DBAZ=2" }, config);
            BOOST_TEST(has(adjusted, "-D__MRDOCS__"));
            BOOST_TEST(has(adjusted, "-DFOO"));
            BOOST_TEST(has(adjusted, "-DBAR=1"));
            BOOST_TEST(has(adjusted, "-DBAZ=2"));
        }

        {
            std::shared_ptr<TestConfigImpl> config = std::make_shared<TestConfigImpl>();
            config->settings_.useSystemStdlib = false;
            config->settings_.stdlibIncludes.push_back("stdlib-path");

            auto adjusted = adjustCompileCommand({ programName }, config);
            BOOST_TEST(has(adjusted, "-X"));
            BOOST_TEST(has(adjusted, { "-external:I", "stdlib-path" }));
        }

        {
            std::shared_ptr<TestConfigImpl> config = std::make_shared<TestConfigImpl>();
            config->settings_.useSystemLibc = false;
            config->settings_.libcIncludes.push_back("libc-path");

            auto adjusted = adjustCompileCommand({ programName }, config);
            BOOST_TEST(has(adjusted, "-nostdinc"));
            BOOST_TEST(has(adjusted, { "-external:I", "libc-path" }));
        }

        {
            std::shared_ptr<TestConfigImpl> config = std::make_shared<TestConfigImpl>();
            config->settings_.systemIncludes.push_back("system-path");

            auto adjusted = adjustCompileCommand({ programName }, config);
            BOOST_TEST(has(adjusted, { "-external:I", "system-path" }));
        }
    }

    void run()
    {
        testClang();
        testClangCL();
    }
};

TEST_SUITE(
    MrDocsCompilationDatabase_test,
    "clang.mrdocs.MrDocsCompilationDatabase");

} // mrdocs
} // clang
