//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/CompilerInfo.hpp>
#include <test_suite/test_suite.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <string>
#include <vector>

namespace mrdocs {

struct CompilerInfo_test
{
    void
    testFallbackCandidates()
    {
        std::vector<std::string> const common = {
            "g++", "clang++", "c++", "gcc", "clang"};

        {
            auto const res = fallbackCompilerCandidates("", "");
            BOOST_TEST(res == common);
        }
        {
            auto const res = fallbackCompilerCandidates(
                "/usr/bin/g++-15", "/usr/bin/gcc-15");
            std::vector<std::string> expected = {
                "/usr/bin/g++-15", "/usr/bin/gcc-15"};
            expected.insert(expected.end(), common.begin(), common.end());
            BOOST_TEST(res == expected);
        }
        {
            auto const res = fallbackCompilerCandidates("", "gcc-15");
            std::vector<std::string> expected = {"gcc-15"};
            expected.insert(expected.end(), common.begin(), common.end());
            BOOST_TEST(res == expected);
        }
    }

    void
    testResolveCompilerPath()
    {
        // A bare name that exists on the PATH resolves to an absolute path.
        {
            auto const res = resolveCompilerPath("sh", "", "");
            BOOST_TEST(res.has_value());
            if (res)
            {
                BOOST_TEST(llvm::sys::path::is_absolute(*res));
                BOOST_TEST(llvm::sys::fs::exists(*res));
            }
        }
        // A bare name that is not on the PATH has no path.
        {
            auto const res = resolveCompilerPath(
                "mrdocs-no-such-compiler-7f3a", "", "");
            BOOST_TEST(!res.has_value());
        }
        // A path with a separator resolves against the command's directory.
        {
            auto const res = resolveCompilerPath(
                "/mrdocs-no-such-dir-7f3a/clang++", "", "");
            BOOST_TEST(!res.has_value());
        }
        {
            auto const res = resolveCompilerPath("", "", "");
            BOOST_TEST(!res.has_value());
        }
    }

    void
    run()
    {
        testResolveCompilerPath();
        testFallbackCandidates();
    }
};

TEST_SUITE(
    CompilerInfo_test,
    "clang.mrdocs.CompilerInfo");

} // namespace mrdocs
