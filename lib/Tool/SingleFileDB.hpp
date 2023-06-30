//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_TOOL_SINGLEFILEDB_HPP
#define MRDOX_TOOL_SINGLEFILEDB_HPP

#include <clang/Tooling/CompilationDatabase.h>
#include <string>
#include <utility>
#include <vector>

namespace clang {
namespace mrdox {

/** Compilation database for a single .cpp file.
*/
class SingleFileDB
    : public tooling::CompilationDatabase
{
    std::vector<tooling::CompileCommand> cc_;

public:
    SingleFileDB(
        llvm::StringRef dir,
        llvm::StringRef file)
    {
        std::vector<std::string> cmds;
        cmds.emplace_back("clang");
        cmds.emplace_back("-fsyntax-only");
        cmds.emplace_back("-std=c++20");
        cmds.emplace_back("-pedantic-errors");
        cmds.emplace_back("-Werror");
        cmds.emplace_back(file);
        cc_.emplace_back(
            dir,
            file,
            std::move(cmds),
            dir);
        cc_.back().Heuristic = "unit test";
    }

    std::vector<tooling::CompileCommand>
    getCompileCommands(
        llvm::StringRef FilePath) const override
    {
        if(! FilePath.equals(cc_.front().Filename))
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

} // mrdox
} // clang

#endif
