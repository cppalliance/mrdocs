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

Error
setupAddonsDir(
    llvm::cl::opt<std::string>& addonsDirArg,
    char const* argv0,
    void* addressOfMain)
{
    namespace fs = llvm::sys::fs;
    using Process = llvm::sys::Process;

    std::string addonsDir;

    // Set addons dir
    if(! addonsDirArg.getValue().empty())
    {
        // from command line
        auto absPath = files::makeAbsolute(
            addonsDirArg.getValue());
        if(! absPath)
            return absPath.error();
        addonsDir = files::makeDirsy(files::normalizePath(*absPath));
        if(auto err = files::requireDirectory(addonsDir))
            return err;
        addonsDirArg.getValue() = addonsDir;
    }
    else
    {
        // check process working directory
        addonsDir = fs::getMainExecutable(argv0, addressOfMain);
        if(addonsDir.empty())
            return Error("getMainExecutable failed");
        addonsDir = files::makeDirsy(files::appendPath(
            files::getParentDir(addonsDir), "addons"));
        if(! files::requireDirectory(addonsDir).failed())
        {
            // from directory containing the process executable
            addonsDirArg.getValue() = addonsDir;
        }
        else
        {
            auto addonsEnvVar = Process::GetEnv("MRDOCS_ADDONS_DIR");
            if(! addonsEnvVar.has_value())
                return Error("no MRDOCS_ADDONS_DIR in environment");

            // from environment variable
            addonsDir = files::makeDirsy(files::normalizePath(*addonsEnvVar));
            if(auto err = files::requireAbsolute(addonsDir))
                return err;
            if(auto err = files::requireDirectory(addonsDir))
                return err;
            addonsDirArg.getValue() = addonsDir;
        }
    }
    return Error::success();
}

} // mrdocs
} // clang
