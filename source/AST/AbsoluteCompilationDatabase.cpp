//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "Support/Debug.hpp"
#include "Support/Path.hpp"
#include "AST/AbsoluteCompilationDatabase.hpp"
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

namespace clang {
namespace mrdox {

AbsoluteCompilationDatabase::
AbsoluteCompilationDatabase(
    llvm::StringRef workingDir,
    CompilationDatabase const& inner)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    auto allCommands = inner.getAllCompileCommands();
    AllCommands_.reserve(allCommands.size());
    SmallPathString temp;
    for(auto const& cmd0 : allCommands)
    {
        tooling::CompileCommand cmd;

        cmd.CommandLine = cmd0.CommandLine;
        cmd.Heuristic = cmd0.Heuristic;
        cmd.Output = cmd0.Output;
        cmd.CommandLine = cmd0.CommandLine;

        if(path::is_absolute(cmd0.Directory))
        {
            path::native(cmd0.Directory, temp);
            cmd.Directory = static_cast<std::string>(temp);
        }
        else
        {
            temp = cmd0.Directory;
            fs::make_absolute(workingDir, temp);
            path::remove_dots(temp, true);
            cmd.Directory = static_cast<std::string>(temp);
        }

        if(path::is_absolute(cmd0.Filename))
        {
            path::native(cmd0.Filename, temp);
            cmd.Filename = static_cast<std::string>(temp);
        }
        else
        {
            temp = cmd0.Filename;
            fs::make_absolute(workingDir, temp);
            path::remove_dots(temp, true);
            cmd.Filename = static_cast<std::string>(temp);
        }

        std::size_t i = AllCommands_.size();
        auto result = IndexByFile_.try_emplace(cmd.Filename, i);
        AllCommands_.emplace_back(std::move(cmd));
        Assert(result.second);
    }
}

std::vector<tooling::CompileCommand>
AbsoluteCompilationDatabase::
getCompileCommands(
    llvm::StringRef FilePath) const
{
    SmallPathString nativeFilePath;
    llvm::sys::path::native(FilePath, nativeFilePath);

    auto const it = IndexByFile_.find(nativeFilePath);
    if (it == IndexByFile_.end())
        return {};
    std::vector<tooling::CompileCommand> Commands;
    Commands.push_back(AllCommands_[it->getValue()]);
    return Commands;
}

std::vector<std::string>
AbsoluteCompilationDatabase::
getAllFiles() const
{
    std::vector<std::string> allFiles;
    allFiles.reserve(AllCommands_.size());
    for(auto const& cmd : AllCommands_)
        allFiles.push_back(cmd.Filename);
    return allFiles;
}

std::vector<tooling::CompileCommand>
AbsoluteCompilationDatabase::
getAllCompileCommands() const
{
    return AllCommands_;
}

} // mrdox
} // clang
