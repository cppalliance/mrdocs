//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Fernando Pelliccioni (fpelliccioni@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "StdLib.hpp"
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Path.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Process.h>

namespace clang {
namespace mrdocs {

Expected<void>
setupStdLibDirs(
    std::vector<std::string>& stdLibDirsArg,
    std::string_view execPath)
{
    if (stdLibDirsArg.empty())
    {
        MRDOCS_CHECK(execPath, "getMainExecutable failed");

        // Set LibC++ path from process working directory
        std::string binDir = files::getParentDir(execPath);
        report::info("execPath: {}", execPath);
        report::info("binDir:   {}", binDir);
        {
            std::string stdLibDir = files::makeDirsy(files::appendPath(
                    binDir, "..", "share", "mrdocs", "libcxx"));
            report::info("Adding libcxx stdlib dir: {}", stdLibDir);
            Error err = files::requireDirectory(stdLibDir);
            MRDOCS_CHECK(err);
            stdLibDirsArg.push_back(stdLibDir);
        }
        {
            std::string clangDir = files::makeDirsy(files::appendPath(
                    binDir, "..", "share", "mrdocs", "clang"));
            report::info("Adding clang stdlib dir: {}", clangDir);
            Error err = files::requireDirectory(clangDir);
            MRDOCS_CHECK(err);
            stdLibDirsArg.push_back(clangDir);
        }
    }
    else
    {
        for (auto& path : stdLibDirsArg)
        {
            path = files::makeDirsy(files::normalizePath(path));
        }
    }
    return {};
}

} // mrdocs
} // clang
