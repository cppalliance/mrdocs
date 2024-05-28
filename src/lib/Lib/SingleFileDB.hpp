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

/** Compilation database for a single .cpp file.
*/
class SingleFileDB
    : public tooling::CompilationDatabase
{
    std::vector<tooling::CompileCommand> cc_;

public:
    explicit
    SingleFileDB(
        llvm::StringRef pathName)
    {
        auto fileName = files::getFileName(pathName);
        auto parentDir = files::getParentDir(pathName);

        std::vector<std::string> cmds;
        cmds.emplace_back("clang");
        cmds.emplace_back("-fsyntax-only");
        cmds.emplace_back("-std=c++23");
        cmds.emplace_back("-pedantic-errors");
        cmds.emplace_back("-Werror");
        cmds.emplace_back(fileName);
        cc_.emplace_back(
            parentDir,
            fileName,
            std::move(cmds),
            parentDir);
        cc_.back().Heuristic = "unit test";
    }

    std::vector<tooling::CompileCommand>
    getCompileCommands(
        llvm::StringRef FilePath) const override
    {
        if (FilePath != cc_.front().Filename)
            return {};
        return { cc_.front() };
    }

    std::vector<std::string>
    getAllFiles() const override
    {
        return { cc_.front().Filename };
    }

    std::vector<tooling::CompileCommand>
    getAllCompileCommands() const override
    {
        return { cc_.front() };
    }
};

} // mrdocs
} // clang

#endif
