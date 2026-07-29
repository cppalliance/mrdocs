//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Matheus Izvekov (mizvekov@gmail.com)
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2025 Agustin K-ballo Berge (agustinberge@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/MrDocsCompilationDatabase.hpp>
#include <mrdocs/SingleFileDB.hpp>
#include <mrdocs/Config.hpp>
#include <mrdocs/Support/Concurrency/ThreadPool.hpp>
#include <mrdocs/Support/Container/Algorithm.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <test_suite/test_suite.hpp>

namespace mrdocs {

struct MrDocsCompilationDatabase_test {
    auto
    adjustCompileCommand(
        std::vector<std::string> commandLine,
        Config const& config) const
    {
        clang::tooling::CompileCommand cc;
        cc.Directory = ".";
        cc.Filename = "test.cpp"; // file does not exist
        cc.CommandLine = std::move(commandLine);
        cc.CommandLine.push_back(cc.Filename);
        cc.Heuristic = "unit test";

        // Create an adjusted MrDocsDatabase
        std::unordered_map<std::string, std::vector<std::string>>
            defaultIncludePaths;
        MrDocsCompilationDatabase compilations(
            "",
            SingleFileDB(std::move(cc)),
            config,
            defaultIncludePaths);
        return compilations.getAllCompileCommands().front().CommandLine;
    }

    auto
    adjustCompileCommand(std::vector<std::string> commandLine) const
    {
        return adjustCompileCommand(std::move(commandLine), Config{});
    }

    static bool
    has(std::vector<std::string> const& commandLine, std::string_view flag)
    {
        return mrdocs::contains(commandLine, flag);
    }
    static bool
    has(std::vector<std::string> const& commandLine,
        std::initializer_list<std::string_view> flags)
    {
        MRDOCS_ASSERT(flags.size() > 0);
        return std::search(
                   commandLine.begin(),
                   commandLine.end(),
                   flags.begin(),
                   flags.end())
               != commandLine.end();
    }

    void
    testClang_stdCxx()
    {
        std::string const programName = "clang";

        {
            auto adjusted = adjustCompileCommand({ programName });
            BOOST_TEST(has(adjusted, "-std=c++26"));
        }
        {
            auto adjusted = adjustCompileCommand({ programName, "-std=c++11" });
            BOOST_TEST(has(adjusted, "-std=c++11"));
            BOOST_TEST_NOT(has(adjusted, "-std=c++23"));
        }
        {
            auto adjusted = adjustCompileCommand(
                { programName, "--std=c++11" });
            BOOST_TEST(has(adjusted, "--std=c++11"));
            BOOST_TEST_NOT(has(adjusted, "-std=c++23"));
        }
    }

    void
    testClang_stdC()
    {
        std::string const programName = "clang";

        {
            auto adjusted = adjustCompileCommand({ programName, "-x", "c" });
            BOOST_TEST(has(adjusted, "-std=c23"));
        }
        {
            auto adjusted = adjustCompileCommand(
                { programName, "-x", "c", "-std=c11" });
            BOOST_TEST(has(adjusted, "-std=c11"));
            BOOST_TEST_NOT(has(adjusted, "-std=c23"));
        }
        {
            auto adjusted = adjustCompileCommand(
                { programName, "-x", "c", "--std=c11" });
            BOOST_TEST(has(adjusted, "--std=c11"));
            BOOST_TEST_NOT(has(adjusted, "-std=c23"));
        }
    }

    void
    testClang_defines()
    {
        std::string const programName = "clang";

        {
            Config config;
            config.defines = { "FOO", "BAR=1" };

            auto adjusted
                = adjustCompileCommand({ programName, "-DBAZ=2" }, config);
            BOOST_TEST(has(adjusted, "-D__MRDOCS__"));
            BOOST_TEST(has(adjusted, "-DFOO"));
            BOOST_TEST(has(adjusted, "-DBAR=1"));
            BOOST_TEST(has(adjusted, "-DBAZ=2"));
        }
    }

    void
    testClang_stdlib()
    {
        std::string const programName = "clang";

        {
            Config config;
            config.useSystemStdlib = false;
            config.stdlibIncludes.push_back("stdlib-path");

            auto adjusted = adjustCompileCommand({ programName }, config);
            BOOST_TEST(has(adjusted, "-nostdinc++"));
            BOOST_TEST(has(adjusted, { "-isystem", "stdlib-path" }));
        }
    }

    void
    testClang_libc()
    {
        std::string const programName = "clang";

        {
            Config config;
            config.useSystemLibc = false;
            config.libcIncludes.push_back("libc-path");

            auto adjusted = adjustCompileCommand({ programName }, config);
            BOOST_TEST(has(adjusted, "-nostdlibinc"));
            BOOST_TEST(has(adjusted, { "-idirafter", "libc-path" }));
        }
    }

    void
    testClang_systemIncludes()
    {
        std::string const programName = "clang";

        {
            Config config;
            config.systemIncludes.push_back("system-path");

            auto adjusted = adjustCompileCommand({ programName }, config);
            BOOST_TEST(has(adjusted, { "-isystem", "system-path" }));
        }
    }

    void
    testClang()
    {
        testClang_stdCxx();
        testClang_stdC();
        testClang_defines();
        testClang_stdlib();
        testClang_libc();
        testClang_systemIncludes();
    }

    void
    testClangCL_stdCxx()
    {
        std::string const programName = "clang-cl";

        {
            auto adjusted = adjustCompileCommand({ programName });
            BOOST_TEST(has(adjusted, "-std:c++latest"));
        }
        {
            auto adjusted = adjustCompileCommand({ programName, "-std:c++11" });
            BOOST_TEST(has(adjusted, "-std:c++11"));
            BOOST_TEST_NOT(has(adjusted, "-std:c++latest"));
        }
        {
            auto adjusted = adjustCompileCommand({ programName, "/std:c++11" });
            BOOST_TEST(has(adjusted, "/std:c++11"));
            BOOST_TEST_NOT(has(adjusted, "-std:c++latest"));
        }
    }

    void
    testClangCL_stdC()
    {
        std::string const programName = "clang-cl";

        {
            auto adjusted = adjustCompileCommand({ programName, "-x", "c" });
            BOOST_TEST(has(adjusted, "-std:clatest"));
        }
        {
            auto adjusted = adjustCompileCommand(
                { programName, "-x", "c", "-std:c11" });
            BOOST_TEST(has(adjusted, "-std:c11"));
            BOOST_TEST_NOT(has(adjusted, "-std:clatest"));
        }
        {
            auto adjusted = adjustCompileCommand(
                { programName, "-x", "c", "/std:c11" });
            BOOST_TEST(has(adjusted, "/std:c11"));
            BOOST_TEST_NOT(has(adjusted, "-std:clatest"));
        }
    }

    void
    testClangCL_defines()
    {
        std::string const programName = "clang-cl";

        {
            Config config;
            config.defines = { "FOO", "BAR=1" };

            auto adjusted
                = adjustCompileCommand({ programName, "-DBAZ=2" }, config);
            BOOST_TEST(has(adjusted, "-D__MRDOCS__"));
            BOOST_TEST(has(adjusted, "-DFOO"));
            BOOST_TEST(has(adjusted, "-DBAR=1"));
            BOOST_TEST(has(adjusted, "-DBAZ=2"));
        }
    }

    void
    testClangCL_stdlib()
    {
        std::string const programName = "clang-cl";

        {
            Config config;
            config.useSystemStdlib = false;
            config.stdlibIncludes.push_back("stdlib-path");

            auto adjusted = adjustCompileCommand({ programName }, config);
            BOOST_TEST(has(adjusted, "-X"));
            BOOST_TEST(has(adjusted, { "-external:I", "stdlib-path" }));
        }
    }

    void
    testClangCL_libc()
    {
        std::string const programName = "clang-cl";

        {
            Config config;
            config.useSystemLibc = false;
            config.libcIncludes.push_back("libc-path");

            auto adjusted = adjustCompileCommand({ programName }, config);
            BOOST_TEST(has(adjusted, "-X"));
            BOOST_TEST(has(adjusted, { "-external:I", "libc-path" }));
        }
    }

    void
    testClangCL_systemIncludes()
    {
        std::string const programName = "clang-cl";

        {
            Config config;
            config.systemIncludes.push_back("system-path");

            auto adjusted = adjustCompileCommand({ programName }, config);
            BOOST_TEST(has(adjusted, { "-external:I", "system-path" }));
        }
    }

    void
    testClangCL()
    {
        testClangCL_stdCxx();
        testClangCL_stdC();
        testClangCL_defines();
        testClangCL_stdlib();
        testClangCL_libc();
        testClangCL_systemIncludes();
    }

    void
    run()
    {
        testClang();
        testClangCL();
    }
};

TEST_SUITE(
    MrDocsCompilationDatabase_test,
    "clang.mrdocs.MrDocsCompilationDatabase");

} // namespace mrdocs
