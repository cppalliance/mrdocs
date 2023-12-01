//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "Addons.hpp"
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Path.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Process.h>

namespace clang {
namespace mrdocs {

Expected<void>
setupAddonsDir(
    llvm::cl::opt<std::string>& addonsDirArg,
    char const* argv0,
    void* addressOfMain)
{
    // Set addons dir from command line argument
    if (!addonsDirArg.getValue().empty())
    {
        MRDOCS_TRY(
            std::string absPath,
            files::makeAbsolute(addonsDirArg.getValue()));
        std::string addonsDir = files::makeDirsy(
            files::normalizePath(absPath));
        MRDOCS_TRY(files::requireDirectory(addonsDir));
        addonsDirArg.getValue() = addonsDir;
        return {};
    }

    // Set addons dir from environment variable
    auto addonsEnvVar = llvm::sys::Process::GetEnv("MRDOCS_ADDONS_DIR");
    if (addonsEnvVar)
    {
        MRDOCS_CHECK(*addonsEnvVar, "MRDOCS_ADDONS_DIR is empty");
        std::string addonsDir = files::makeDirsy(files::normalizePath(*addonsEnvVar));
        MRDOCS_TRY(files::requireAbsolute(addonsDir));
        MRDOCS_TRY(files::requireDirectory(addonsDir));
        addonsDirArg.getValue() = addonsDir;
        return {};
    }

    // Set addons dir from process working directory
    std::string execPath = llvm::sys::fs::getMainExecutable(argv0, addressOfMain);
    MRDOCS_CHECK(execPath, "getMainExecutable failed");
    std::string binDir = files::getParentDir(execPath);
    std::string addonsDir = files::makeDirsy(files::appendPath(
            binDir, "..", "share", "mrdocs", "addons"));
    Error err = files::requireDirectory(addonsDir);
    MRDOCS_CHECK(err);
    addonsDirArg.getValue() = addonsDir;
    return {};
}

} // mrdocs
} // clang
