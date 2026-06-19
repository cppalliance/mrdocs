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

#include <lib/Support/Path.hpp>
#include <clang/Tooling/CompilationDatabase.h>
#include <string>
#include <utility>
#include <vector>


namespace mrdocs {

/** Compilation database for a single file.
*/
class SingleFileDB
    : public clang::tooling::CompilationDatabase
{
    clang::tooling::CompileCommand cc_;

public:
    explicit SingleFileDB(clang::tooling::CompileCommand cc)
        : cc_(std::move(cc))
    {}

    /** Build a database for one source file from a compiler command line.

        The file's directory and name are derived from `pathName`, and the
        file name is appended to `cmds` to form the full command. The
        command's heuristic is tagged "manual" since it is synthesized
        rather than read from a real compilation database.

        @param pathName Path to the single source file.
        @param cmds The compiler command line, without the source file.
    */
    static
    SingleFileDB
    create(llvm::StringRef pathName, std::vector<std::string> cmds)
    {
        auto fileName = files::getFileName(pathName);
        auto parentDir = files::getParentDir(pathName);

        cmds.emplace_back(fileName);

        clang::tooling::CompileCommand
            cc(parentDir, fileName, std::move(cmds), parentDir);
        cc.Heuristic = "manual";
        return SingleFileDB(std::move(cc));
    }

    std::vector<clang::tooling::CompileCommand>
    getCompileCommands(
        llvm::StringRef FilePath) const override
    {
        if (FilePath != cc_.Filename)
        {
            return {};
        }
        return { cc_ };
    }

    std::vector<std::string>
    getAllFiles() const override
    {
        return { cc_.Filename };
    }

    std::vector<clang::tooling::CompileCommand>
    getAllCompileCommands() const override
    {
        return { cc_ };
    }
};

} // mrdocs


#endif
