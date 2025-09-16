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

/** Compilation database for a single .cpp file.
*/
class SingleFileTestDB
    : public tooling::CompilationDatabase
{
    tooling::CompileCommand cc_;

public:
    explicit
    SingleFileTestDB(
        tooling::CompileCommand cc)
        : cc_(std::move(cc))
    {}

    std::vector<tooling::CompileCommand>
    getCompileCommands(
        llvm::StringRef FilePath) const override
    {
        if (FilePath != cc_.Filename)
            return {};
        return { cc_ };
    }

    std::vector<std::string>
    getAllFiles() const override
    {
        return { cc_.Filename };
    }

    std::vector<tooling::CompileCommand>
    getAllCompileCommands() const override
    {
        return { cc_ };
    }
};

struct MrDocsCompilationDatabase_test
{
    Config::Settings fileSettings_;
    ThreadPool threadPool_;
    ReferenceDirectories dirs_;

    std::shared_ptr<ConfigImpl const> config_ =
        ConfigImpl::load(fileSettings_, dirs_, threadPool_).value();

    auto adjustCompileCommand(std::vector<std::string> CommandLine) const
    {
        tooling::CompileCommand cc;
        cc.Directory = ".";
        cc.Filename = "test.cpp"; // file does not exist
        cc.CommandLine = std::move(CommandLine);
        cc.CommandLine.push_back(cc.Filename);
        cc.Heuristic = "unit test";

        // Create an adjusted MrDocsDatabase
        std::unordered_map<std::string, std::vector<std::string>> defaultIncludePaths;
        MrDocsCompilationDatabase compilations(
            llvm::StringRef(cc.Directory), SingleFileTestDB(cc), config_, defaultIncludePaths);
        return compilations.getCompileCommands(cc.Filename).front().CommandLine;
    }

    void testClangCL()
    {
         auto adjusted = adjustCompileCommand({ "clang-cl", "/std:c++latest"});
         BOOST_TEST_GE(adjusted.size(), 2);
         BOOST_TEST_EQ(adjusted[0], "clang");
         BOOST_TEST(contains(adjusted, "-std=c++23"));
    }

    void run()
    {
        testClangCL();
    }
};

TEST_SUITE(
    MrDocsCompilationDatabase_test,
    "clang.mrdocs.MrDocsCompilationDatabase");

} // mrdocs
} // clang
