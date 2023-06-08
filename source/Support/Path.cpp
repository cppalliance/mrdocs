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

#include "Path.hpp"
#include "Support/Debug.hpp"
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/MemoryBuffer.h>

#include <fstream>

namespace clang {
namespace mrdox {

static
bool
isDirsy(
    std::string_view nativeDirName)
{
    namespace path = llvm::sys::path;
    if(nativeDirName.empty())
        return false;
    if(! path::is_separator(
        nativeDirName.back(),
        path::Style::windows_slash))
        return false;
    return true;
}

std::string_view
makeDirsy(
    std::string& dirName)
{
    namespace path = llvm::sys::path;

    if( ! dirName.empty() &&
        ! path::is_separator(
            dirName.back(),
            path::Style::windows_slash))
    {
        auto const sep = path::get_separator(path::Style::native);
        dirName.push_back(sep.front());
    }

    return dirName;
}

std::string
makeAbsoluteDirectory(
    std::string_view dirName,
    std::string_view workingDir)
{
    namespace path = llvm::sys::path;

    Assert(isDirsy(workingDir));

    if(! path::is_absolute(dirName, path::Style::windows_slash))
    {
        SmallPathString result(workingDir);
        path::append(result, path::Style::native, dirName);
        path::remove_dots(result, true, path::Style::native);
        makeDirsy(result);
        return std::string(result);
    }

    SmallPathString result(workingDir);
    path::remove_dots(result, true, path::Style::native);
    makeDirsy(result);
    return std::string(result);
}

std::string
makeFilePath(
    std::string_view dirName,
    std::string_view fileName)
{
    namespace path = llvm::sys::path;

    SmallPathString result(dirName);
    path::append(result, path::Style::native, fileName);
    path::remove_dots(result, true, path::Style::native);
    return std::string(result);
}

//------------------------------------------------

llvm::StringRef
convert_to_slash(
    llvm::SmallVectorImpl<char> &path,
    llvm::sys::path::Style style)
{
    if (! llvm::sys::path::is_style_posix(style))
        std::replace(path.begin(), path.end(), '\\', '/');
    return llvm::StringRef(path.data(), path.size());
}

void
makeDirsy(
    llvm::SmallVectorImpl<char>& s,
    llvm::sys::path::Style style)
{
    namespace path = llvm::sys::path;

    if( ! s.empty() &&
        ! path::is_separator(s.back(), style))
    {
        auto const sep = path::get_separator(style);
        s.insert(s.end(), sep.begin(), sep.end());
    }
}

//------------------------------------------------

Error
forEachFile(
    std::string_view dirPath,
    AnyFileVisitor& visitor)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    std::error_code ec;
    fs::directory_iterator const end{};
    fs::directory_iterator it(dirPath, ec, false);
    if(ec)
        return Error("fs::directory_iterator(\"{}\") returned \"{}\"", dirPath, ec);
    while(it != end)
    {
        if(it->type() == fs::file_type::directory_file)
        {
            std::string s = it->path();
            makeDirsy(s);
            auto err = visitor.visitFile(s);
            if(err)
                return err;
        }
        else if(it->type() == fs::file_type::regular_file)
        {
            auto err = visitor.visitFile(it->path());
            if(err)
                return err;
        }
        else
        {
            // we don't handle this type
        }
        it.increment(ec);
        if(ec)
            return Error("directory_iterator::increment returned \"{}\"", ec);
    }
    return Error::success();
}

//------------------------------------------------

namespace files {

std::string_view
getFileName(
    std::string_view pathName)
{
    namespace path = llvm::sys::path;

    return path::filename(pathName);
}

Expected<std::string>
getFileText(
    std::string_view pathName)
{
    std::ifstream file((std::string(pathName)));
    if(! file.good())
        return Error("std::ifstream(\"{}\" returned \"{}\"",
            pathName, std::error_code(errno, std::generic_category()));
    std::istreambuf_iterator<char> it(file);
    std::istreambuf_iterator<char> const end;
    std::string text(it, end);
    if(! file.good())
        return Error("getFileText(\"{}\") returned \"{}\"",
            pathName, std::error_code(errno, std::generic_category()));
    return text;
}

std::string
makeDirsy(
    std::string_view pathName)
{
    namespace path = llvm::sys::path;

    std::string result = static_cast<std::string>(pathName);
    if( ! result.empty() &&
        ! path::is_separator(
            result.back(),
            path::Style::windows_slash))
    {
        auto const sep = path::get_separator(path::Style::native);
        result.push_back(sep.front());
    }
    return result;
}

std::string
appendPath(
    std::string_view basePath,
    std::string_view pathName)
{
    namespace path = llvm::sys::path;

    SmallPathString temp(makeDirsy(basePath));
    path::append(temp, pathName);
    return static_cast<std::string>(temp.str());
}

} // files

} // mrdox
} // clang
