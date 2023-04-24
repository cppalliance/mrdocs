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

#include "ConfigImpl.hpp"
#include "Support/Path.hpp"
#include <clang/Tooling/AllTUsExecution.h>

namespace clang {
namespace mrdox {

//------------------------------------------------
//
// ConfigImpl
//
//------------------------------------------------

llvm::SmallString<0>
ConfigImpl::
normalizePath(
    llvm::StringRef pathName)
{
    namespace path = llvm::sys::path;

    llvm::SmallString<0> result;
    if(! path::is_absolute(pathName))
    {
        result = configDir();
        path::append(result, path::Style::posix, pathName);
        path::remove_dots(result, true, path::Style::posix);
    }
    else
    {
        result = pathName;
        path::remove_dots(result, true);
        convert_to_slash(result);
    }
    return result;
}

ConfigImpl::
ConfigImpl(
    llvm::StringRef configDir)
    : Config(configDir)
    , threadPool_(
        llvm::hardware_concurrency(
            tooling::ExecutorConcurrency))
{
}

void
ConfigImpl::
setSourceRoot(
    llvm::StringRef dirPath)
{
    namespace path = llvm::sys::path;

    auto temp = normalizePath(dirPath);
    makeDirsy(temp, path::Style::posix);
    sourceRoot_ = temp.str();
}

void
ConfigImpl::
setInputFileIncludes(
    std::vector<std::string> const& list)
{
    namespace path = llvm::sys::path;

    for(auto const& s0 : list)
        inputFileIncludes_.push_back(normalizePath(s0));
}

//------------------------------------------------

bool
ConfigImpl::
shouldVisitTU(
    llvm::StringRef filePath) const noexcept
{
    if(inputFileIncludes_.empty())
        return true;
    for(auto const& s : inputFileIncludes_)
        if(filePath == s)
            return true;
    return false;
}

bool
ConfigImpl::
shouldVisitFile(
    llvm::StringRef filePath,
    llvm::SmallVectorImpl<char>& prefixPath) const noexcept
{
    namespace path = llvm::sys::path;

    llvm::SmallString<32> temp;
    temp = filePath;
    if(! path::replace_path_prefix(temp, sourceRoot_, "", path::Style::posix))
        return false;
    prefixPath.assign(sourceRoot_.begin(), sourceRoot_.end());
    makeDirsy(prefixPath);
    return true;
}

} // mrdox
} // clang
