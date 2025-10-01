//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SINGLEFILEDB_HPP
#define MRDOCS_LIB_SINGLEFILEDB_HPP

#include <mrdocs/Support/Path.hpp>
#include <clang/Tooling/CompilationDatabase.h>
#include <string>
#include <utility>
#include <vector>

namespace clang {
namespace mrdocs {

/** Compilation database for a single file.
*/
class SingleFileDB
    : public tooling::CompilationDatabase
{
    tooling::CompileCommand cc_;

public:
    explicit SingleFileDB(tooling::CompileCommand cc)
        : cc_(std::move(cc))
    {}

    static SingleFileDB makeForClang(llvm::StringRef pathName)
    {
        auto fileName = files::getFileName(pathName);
        auto parentDir = files::getParentDir(pathName);

        std::vector<std::string> cmds = {"clang",
            "-std=c++23", "-pedantic-errors", "-Werror", std::string{fileName}};
        tooling::CompileCommand cc(
            parentDir,
            fileName,
            std::move(cmds),
            parentDir);
        cc.Heuristic = "unit test";
        return SingleFileDB(std::move(cc));
    }

    static SingleFileDB makeForClangCL(llvm::StringRef pathName)
    {
        auto fileName = files::getFileName(pathName);
        auto parentDir = files::getParentDir(pathName);

        std::vector<std::string> cmds = {"clang-cl",
            "/std:c++latest", "/permissive-", "/WX", std::string{fileName}};
        tooling::CompileCommand cc(
            parentDir,
            fileName,
            std::move(cmds),
            parentDir);
        cc.Heuristic = "unit test";
        return SingleFileDB(std::move(cc));
    }

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

} // mrdocs
} // clang

#endif
