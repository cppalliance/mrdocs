//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "TestFiles.hpp"

namespace clang {
namespace mrdox {

namespace fs = llvm::sys::fs;
namespace path = llvm::sys::path;

bool
TestFiles::
addDirectory(
    llvm::StringRef path,
    Reporter& R)
{
    std::error_code ec;
    llvm::SmallString<256> dir(path);
    path::remove_dots(dir, true);
    fs::directory_iterator const end{};
    fs::directory_iterator iter(dir, ec, false);
    if(! R.success("addDirectory", ec))
        return false;
    while(iter != end)
    {
        if(iter->type() == fs::file_type::directory_file)
        {
            addDirectory(iter->path(), R);
        }
        else if(
            iter->type() == fs::file_type::regular_file &&
            path::extension(iter->path()).equals_insensitive(".cpp"))
        {
            llvm::SmallString<256> output(iter->path());
            path::replace_extension(output, "xml");
            std::vector<std::string> commandLine = {
                "clang",
                iter->path()
            };
            cc_.emplace_back(
                dir,
                iter->path(),
                std::move(commandLine),
                output);
            cc_.back().Heuristic = "unit test";

        }
        else
        {
            // we don't handle this type
        }
        iter.increment(ec);
        if(! R.success("increment", ec))
            return false;
    }
    return true;
}

std::vector<tooling::CompileCommand>
TestFiles::
getCompileCommands(
    llvm::StringRef FilePath) const
{
    std::vector<tooling::CompileCommand> result;
    for(auto const& cc : cc_)
        if(FilePath.equals(cc.Filename))
            result.push_back(cc);
    return result;
}

std::vector<std::string>
TestFiles::
getAllFiles() const
{
    std::vector<std::string> result;
    result.reserve(cc_.size());
    for(auto const& cc : cc_)
        result.push_back(cc.Filename);
    return result;
}

std::vector<tooling::CompileCommand>
TestFiles::
getAllCompileCommands() const
{
    return cc_;
}

} // mrdox
} // clang
