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
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/MemoryBuffer.h>
#include <fstream>

namespace clang {
namespace mrdox {

AnyFileVisitor::
~AnyFileVisitor() = default;

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
            auto s = files::makeDirsy(it->path());
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
//
// files
//
//------------------------------------------------

namespace files {

bool
isAbsolute(
    std::string_view pathName) noexcept
{
    namespace path = llvm::sys::path;

    return path::is_absolute(pathName);
}

Error
requireAbsolute(
    std::string_view pathName)
{
    if(! isAbsolute(pathName))
        return Error("\"{}\" is not an absolute path");
    return Error::success();
}

bool
isDirsy(
    std::string_view pathName) noexcept
{
    namespace path = llvm::sys::path;

    if(pathName.empty())
        return false;
    if(! path::is_separator(
        pathName.back(),
            path::Style::native))
        return false;
    return true;
}

std::string
normalizePath(
    std::string_view pathName)
{
    namespace path = llvm::sys::path;

    SmallPathString result(pathName);
    path::remove_dots(result, true);
    return static_cast<std::string>(result.str());
}

std::string
getParentDir(
    std::string_view pathName)
{
    namespace path = llvm::sys::path;

    auto result = path::parent_path(pathName).str();
    return makeDirsy(result);
}

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

Expected<std::string>
makeAbsolute(
    std::string_view pathName)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    SmallPathString result(pathName);
    if(auto ec = fs::make_absolute(result))
        return Error("fs::make_absolute(\"{}\") returned \"{}\"", pathName, ec);
    return static_cast<std::string>(result);
}

std::string
makeAbsolute(
    std::string_view pathName,
    std::string_view workingDir)
{
    namespace path = llvm::sys::path;

    MRDOX_ASSERT(isDirsy(workingDir));

    if(! path::is_absolute(pathName))
    {
        SmallPathString result(workingDir);
        path::append(result, path::Style::native, pathName);
        path::remove_dots(result, true);//, path::Style::native);
        return std::string(result);
    }

    SmallPathString result(pathName);
    path::remove_dots(result, true, path::Style::native);
    return std::string(result);
}

std::string
makePosixStyle(
    std::string_view pathName)
{
    std::string result(pathName);
    for(auto& c : result)
        if(c == '\\')
            c = '/';
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
    path::remove_dots(temp, true);
    return static_cast<std::string>(temp.str());
}

Error
requireDirectory(
    std::string_view pathName)
{
    namespace fs = llvm::sys::fs;

    fs::file_status fileStatus;
    if(auto ec = fs::status(pathName, fileStatus))
        return Error("fs::status(\"{}\") returned \"{}\"", pathName, ec);
    if(fileStatus.type() != fs::file_type::directory_file)
        return Error("\"{}\" is not a directory", pathName);
    return Error::success();
}

} // files

} // mrdox
} // clang