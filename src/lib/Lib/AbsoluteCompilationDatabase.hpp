//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_LIB_TOOL_ABSOLUTECOMPILATIONDATABASE_HPP
#define MRDOX_LIB_TOOL_ABSOLUTECOMPILATIONDATABASE_HPP

#include <mrdox/Config.hpp>
#include <clang/Tooling/JSONCompilationDatabase.h>
#include <llvm/ADT/StringMap.h>
#include <vector>

namespace clang {
namespace mrdox {

/** A compilation database where all paths are absolute.

    All relative paths in the compilation database
    will be converted to absolute paths by resolving
    them according to the working directory specified
    at construction.
*/
class AbsoluteCompilationDatabase
    : public tooling::CompilationDatabase
{
    std::vector<tooling::CompileCommand> AllCommands_;
    llvm::StringMap<std::size_t> IndexByFile_;

public:
    /** Constructor.

        This copies the contents of the source compilation
        database. Every relative path is converted into an
        absolute path by resolving against the specified
        working directory.
    */
    AbsoluteCompilationDatabase(
        llvm::StringRef workingDir,
        CompilationDatabase const& inner,
        std::shared_ptr<const Config> config);

    std::vector<tooling::CompileCommand>
    getCompileCommands(
        llvm::StringRef FilePath) const override;

    std::vector<std::string>
    getAllFiles() const override;

    std::vector<tooling::CompileCommand>
    getAllCompileCommands() const override;
};

} // mrdox
} // clang

#endif

