//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_TESTS_SINGLE_FILE_HPP
#define MRDOX_TESTS_SINGLE_FILE_HPP

#include "Config.hpp"

#include <clang/Tooling/CompilationDatabase.h>
#include <string>
#include <utility>
#include <vector>


namespace clang {
namespace mrdox {

/** Compilation database for a single .cpp file.
*/
class SingleFile
    : public tooling::CompilationDatabase
{
    std::vector<tooling::CompileCommand> cc_;

public:
    SingleFile(
        llvm::StringRef dir,
        llvm::StringRef file)
    {
        std::vector<std::string> cmds;
        cmds.emplace_back("clang");
        cmds.emplace_back(MRDOX_STANDARD_OPTION_STRING);
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
