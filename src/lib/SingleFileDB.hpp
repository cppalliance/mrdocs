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
