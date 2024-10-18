//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "Path.hpp"
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/MemoryBuffer.h>
#include <fstream>

namespace clang {
namespace mrdocs {

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
    bool recursive,
    AnyFileVisitor& visitor)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    std::error_code ec;
    fs::directory_iterator const end{};
    fs::directory_iterator it(dirPath, ec, false);
    if(ec)
        return formatError("fs::directory_iterator(\"{}\") returned \"{}\"", dirPath, ec);
    while(it != end)
    {
        if(it->type() == fs::file_type::directory_file)
        {
            auto s = it->path();
            auto err = visitor.visitFile(s);
            if(err)
                return err;

            if(recursive)
            {
                if(auto err = forEachFile(
                    it->path(), recursive, visitor))
                    return err;
            }
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
            return formatError("directory_iterator::increment returned \"{}\"", ec);
    }
    return Error::success();
}

//------------------------------------------------
//
// files
//
//------------------------------------------------

namespace files {

Expected<FileType>
getFileType(
    std::string_view pathName)
{
    namespace fs = llvm::sys::fs;
    fs::file_status fileStatus;
    if(auto ec = fs::status(pathName, fileStatus))
    {
        if(ec == std::errc::no_such_file_or_directory)
            return FileType::not_found;
        return Unexpected(Error(ec));
    }
    switch(fileStatus.type())
    {
    case fs::file_type::regular_file:
        return FileType::regular;

    case fs::file_type::directory_file:
        return FileType::directory;

    case fs::file_type::symlink_file:
    case fs::file_type::block_file:
    case fs::file_type::character_file:
    case fs::file_type::fifo_file:
    case fs::file_type::socket_file:
    case fs::file_type::type_unknown:
        return FileType::other;

    case fs::file_type::file_not_found:
    case fs::file_type::status_error:
    default:
        MRDOCS_UNREACHABLE();
    }
}

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
        return formatError("\"{}\" is not an absolute path");
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
normalizeDir(
    std::string_view pathName)
{
    return normalizePath(pathName);
}

std::string
getParentDir(
    std::string_view pathName)
{
    return llvm::sys::path::parent_path(pathName).str();
}

std::string
getParentDir(
    std::string_view pathName,
    unsigned levels)
{
    std::string res(pathName);
    for (unsigned i = 0; i < levels; ++i)
    {
        res = getParentDir(res);
    }
    return res;
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
        return Unexpected(formatError("std::ifstream(\"{}\" returned \"{}\"",
            pathName, std::error_code(errno, std::generic_category())));
    std::istreambuf_iterator<char> it(file);
    std::istreambuf_iterator<char> const end;
    std::string text(it, end);
    if(! file.good())
        return Unexpected(formatError("getFileText(\"{}\") returned \"{}\"",
            pathName, std::error_code(errno, std::generic_category())));
    return text;
}

std::string
makeDirsy(
    std::string_view pathName)
{
    namespace path = llvm::sys::path;

    std::string result = static_cast<std::string>(pathName);
    if (!result.empty())
    {
        const char c = result.back();
        if (!path::is_separator(c, path::Style::windows_slash))
        {
            auto const sep = path::get_separator(path::Style::native);
            result.push_back(sep.front());
        }
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
        return Unexpected(formatError("fs::make_absolute(\"{}\") returned \"{}\"", pathName, ec));
    return static_cast<std::string>(result);
}

std::string
makeAbsolute(
    std::string_view pathName,
    std::string_view workingDir)
{
    namespace path = llvm::sys::path;

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
withExtension(
    std::string_view fileName,
    std::string_view ext)
{
    namespace path = llvm::sys::path;

    SmallPathString temp(fileName);
    path::replace_extension(
        temp, ext, path::Style::windows_slash);
    return std::string(temp);
}

std::string
appendPath(
    std::string_view basePath,
    std::string_view name)
{
    namespace path = llvm::sys::path;
    SmallPathString temp(basePath);
    path::append(temp, name);
    path::remove_dots(temp, true);
    return static_cast<std::string>(temp.str());
}

std::string
appendPath(
    std::string_view basePath,
    std::string_view name1,
    std::string_view name2)
{
    namespace path = llvm::sys::path;

    SmallPathString temp(basePath);
    path::append(temp, name1, name2);
    path::remove_dots(temp, true);
    return static_cast<std::string>(temp.str());
}

std::string
appendPath(
    std::string_view basePath,
    std::string_view name1,
    std::string_view name2,
    std::string_view name3)
{
    namespace path = llvm::sys::path;

    SmallPathString temp(basePath);
    path::append(temp, name1, name2, name3);
    path::remove_dots(temp, true);
    return static_cast<std::string>(temp.str());
}

std::string
appendPath(
    std::string_view basePath,
    std::string_view name1,
    std::string_view name2,
    std::string_view name3,
    std::string_view name4)
{
    namespace path = llvm::sys::path;

    SmallPathString temp(basePath);
    path::append(temp, name1, name2, name3, name4);
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
        return formatError("fs::status(\"{}\") returned \"{}\"", pathName, ec);
    if(fileStatus.type() != fs::file_type::directory_file)
        return formatError("\"{}\" is not a directory", pathName);
    return Error::success();
}

bool
isDirectory(
    std::string_view pathName)
{
    namespace fs = llvm::sys::fs;
    fs::file_status fileStatus;
    if(auto ec = fs::status(pathName, fileStatus))
        return false;
    if(fileStatus.type() != fs::file_type::directory_file)
        return false;
    return true;
}

bool
exists(
    std::string_view pathName)
{
    namespace fs = llvm::sys::fs;
    fs::file_status fileStatus;
    if(auto ec = fs::status(pathName, fileStatus))
        return false;
    return exists(fileStatus);
}

std::string_view
getSourceFilename(
    std::string_view pathName)
{
    namespace path = llvm::sys::path;
    auto const begin = path::rend(pathName);
    auto it = path::rbegin(pathName);
    auto prev = it;
    while(it != begin)
    {
        if(*it == "source")
        {
            return pathName.substr(prev - begin);
        }
        if(*it == "include")
        {
            return pathName.substr(prev - begin);
        }
        prev = it;
        ++it;
    }
    return pathName;
}

Error
createDirectory(
    std::string_view pathName)
{
    namespace fs = llvm::sys::fs;

    auto kind = files::getFileType(pathName);
    if (!kind)
        return kind.error();
    if(*kind == files::FileType::directory)
        return Error::success();
    else if(*kind != files::FileType::not_found)
        return formatError("creating the directory \"{}\""
            " would overwrite an existing file", pathName);

    if(auto ec = fs::create_directories(pathName))
        return formatError("fs::create_directories(\"{}\") returned {}", pathName, ec);

    return Error::success();
}

bool
startsWith(
    std::string_view pathName,
    std::string_view prefix)
{
    auto itPath = pathName.begin();
    auto itPrefix = prefix.begin();
    while (itPath != pathName.end() && itPrefix != prefix.end()) {
        if (*itPath != *itPrefix) {
            char pathChar = (*itPath == '\\') ? '/' : *itPath;
            char prefixChar = (*itPrefix == '\\') ? '/' : *itPrefix;
            if (pathChar != prefixChar)
            {
                return false;
            }
        }
        ++itPath;
        ++itPrefix;
    }
    // Have we consumed the whole prefix?
    return itPrefix == prefix.end();
}

} // files

ScopedTempFile::
~ScopedTempFile()
{
    if (ok_)
    {
        llvm::sys::fs::remove(path_);
    }
}

ScopedTempFile::
ScopedTempFile(
    llvm::StringRef prefix,
    llvm::StringRef ext)
{
    llvm::SmallString<128> tempPath;
    ok_ = !llvm::sys::fs::createTemporaryFile(prefix, ext, tempPath);
    if (ok_)
    {
        path_ = tempPath;
    }
}

ScopedTempDirectory::
~ScopedTempDirectory() {
    if (ok_)
    {
        llvm::sys::fs::remove_directories(path_);
    }
}

ScopedTempDirectory::
ScopedTempDirectory(
    llvm::StringRef prefix)
{
    llvm::SmallString<128> tempPath;
    ok_ = !llvm::sys::fs::createUniqueDirectory(prefix, tempPath);
    if (ok_)
    {
        path_ = tempPath;
    }
}


} // mrdocs
} // clang