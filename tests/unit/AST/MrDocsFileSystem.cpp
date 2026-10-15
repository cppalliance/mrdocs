//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/AST/MrDocsFileSystem.hpp>
#include <mrdocs/Config.hpp>
#include <mrdocs/Support/Filesystem/Temp.hpp>
#include <test_suite/test_suite.hpp>
#include <llvm/ADT/IntrusiveRefCntPtr.h>
#include <llvm/Support/ErrorOr.h>
#include <llvm/Support/VirtualFileSystem.h>
#include <filesystem>
#include <string>
#include <system_error>

namespace mrdocs {

struct MrDocsFileSystemTest
{
    // Setting the working directory of the file system a unit is parsed
    // with leaves the process's alone, so units parsed concurrently cannot
    // move it under each other.
    void
    testWorkingDirectoryIsItsOwn()
    {
        ScopedTempDirectory td("mrdocs-fs-cwd");
        BOOST_TEST(td);
        std::string const before = std::filesystem::current_path().string();
        Config const config;
        llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> fs =
            createMrDocsFileSystem(config);
        std::error_code const ec = fs->setCurrentWorkingDirectory(td.path());
        BOOST_TEST(!ec);
        BOOST_TEST(std::filesystem::current_path().string() == before);
        llvm::ErrorOr<std::string> const cwd = fs->getCurrentWorkingDirectory();
        BOOST_TEST(!cwd.getError());
        if (cwd)
        {
            BOOST_TEST(*cwd == std::string(td.path()));
        }
        // Should the process have moved, put it back, so the temporary
        // directory can be removed and the later suites run where they
        // expect.
        std::filesystem::current_path(before);
    }

    void
    run()
    {
        testWorkingDirectoryIsItsOwn();
    }
};

TEST_SUITE(
    MrDocsFileSystemTest,
    "clang.mrdocs.AST.MrDocsFileSystem");

} // namespace mrdocs
