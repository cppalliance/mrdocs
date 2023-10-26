//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_PATH_HPP
#define MRDOCS_API_SUPPORT_PATH_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Support/Error.hpp>
#include <string>
#include <string_view>

namespace clang {
namespace mrdocs {

//------------------------------------------------

struct MRDOCS_VISIBLE
    AnyFileVisitor
{
    virtual ~AnyFileVisitor() = 0;
    virtual Error visitFile(std::string_view fileName) = 0;
};

/** Call a function for each file in a directory.

    This will iterate all the regular files in
    a directory and invoke the visitor with the
    path.
*/
MRDOCS_DECL
Error
forEachFile(
    std::string_view dirPath,
    AnyFileVisitor& visitor);

/** Visit each file in a directory.
*/
template<class Visitor>
Error
forEachFile(
    std::string_view dirPath,
    Visitor&& visitor)
{
    struct FileVisitor : AnyFileVisitor
    {
        Visitor& visitor_;

        explicit FileVisitor(Visitor& v)
            : visitor_(v)
        {
        }

        Error
        visitFile(std::string_view fileName) override
        {
            return visitor_(fileName);
        }
    };

    FileVisitor v{visitor};
    return forEachFile(dirPath, static_cast<AnyFileVisitor&>(v));
}

//------------------------------------------------

namespace files {

enum class FileType
{
    not_found,
    regular,
    directory,
    other
};

/** Return the file type or an error

    @param pathName The absolute or relative path
    to the file.
*/
MRDOCS_DECL
Expected<FileType>
getFileType(
    std::string_view pathName);

/** Return true if pathName is absolute.
*/
MRDOCS_DECL
bool
isAbsolute(
    std::string_view pathName) noexcept;

/** Return an error if pathName is not absolute.
*/
MRDOCS_DECL
Error
requireAbsolute(
    std::string_view pathName);

/** Return true if pathName ends in a separator.
*/
MRDOCS_DECL
bool
isDirsy(
    std::string_view pathName) noexcept;

/** Return a normalized path.

    This function returns a new path based on
    applying the following changes to the passed
    path:

    @li "." and ".." are resolved

    @li Separators made uniform

    @return The normalized path.

    @param pathName The relative or absolute path.
*/
MRDOCS_DECL
std::string
normalizePath(
    std::string_view pathName);

/** Return the parent directory.

    If the parent directory is defined, the returned
    path will always have a trailing separator.
*/
MRDOCS_DECL
std::string
getParentDir(
    std::string_view pathName);

/** Return the filename part of the path.
*/
MRDOCS_DECL
std::string_view
getFileName(
    std::string_view pathName);

/** Return the contents of a file as a string.
*/
MRDOCS_DECL
Expected<std::string>
getFileText(
    std::string_view pathName);

/** Append a trailing native separator if not already present.
*/
MRDOCS_DECL
std::string
makeDirsy(
    std::string_view pathName);

/** Return an absolute path from a possibly relative path.

    Relative paths are resolved against the
    current working directory of the process.

    @return The absolute path, or an error if
    any occurred.
*/
MRDOCS_DECL
Expected<std::string>
makeAbsolute(
    std::string_view pathName);

/** Return an absolute path from a possibly relative path.
*/
MRDOCS_DECL
std::string
makeAbsolute(
    std::string_view pathName,
    std::string_view workingDir);

/** Convert all backward slashes to forward slashes.
*/
MRDOCS_DECL
std::string
makePosixStyle(
    std::string_view pathName);

/** Return the filename with a new or different extension.

    @param fileName The absolute or relative path
    to the directory or file.

    @param ext The extension to use, without a
    leading dot. If this is empty and the path
    contains an extension, then the extension is
    removed.
*/
MRDOCS_DECL
std::string
withExtension(
    std::string_view fileName,
    std::string_view ext);

MRDOCS_DECL
std::string
appendPath(
    std::string_view basePath,
    std::string_view name);

MRDOCS_DECL
std::string
appendPath(
    std::string_view basePath,
    std::string_view name1,
    std::string_view name2);

MRDOCS_DECL
std::string
appendPath(
    std::string_view basePath,
    std::string_view name1,
    std::string_view name2,
    std::string_view name3);

/** Return an error if the path is not a directory.
*/
MRDOCS_DECL
Error
requireDirectory(
    std::string_view pathName);

/** Return the relevant suffix of a source file path.
*/
MRDOCS_DECL
std::string_view
getSourceFilename(
    std::string_view pathName);

/** Create a directory.

    Any missing parent directories will also be created.

    @param pathName The absolute or relative path
    to create.
*/
MRDOCS_DECL
Error
createDirectory(
    std::string_view pathName);

} // files

} // mrdocs
} // clang

#endif