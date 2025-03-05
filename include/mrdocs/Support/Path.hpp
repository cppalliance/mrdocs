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
#include <mrdocs/Support/Expected.hpp>
#include <string>
#include <string_view>

namespace clang {
namespace mrdocs {

//------------------------------------------------

struct MRDOCS_VISIBLE
    AnyFileVisitor
{
    virtual
    ~AnyFileVisitor() = 0;

    virtual
    Expected<void>
    visitFile(std::string_view fileName) = 0;
};

/** Call a function for each file in a directory.

    This will iterate all the regular files in
    a directory and invoke the visitor with the
    path.

    @param recursive If true, files in subdirectories are
    also visited, recursively.
*/
MRDOCS_DECL
Expected<void>
forEachFile(
    std::string_view dirPath,
    bool recursive,
    AnyFileVisitor& visitor);

namespace detail {
template <class Visitor>
class FileVisitor : public AnyFileVisitor
{
    using R = std::invoke_result_t<Visitor, std::string_view>;

public:
    Visitor& visitor_;

    explicit FileVisitor(Visitor& v)
        : visitor_(v)
    {
    }

    Expected<void>
    visitFile(std::string_view fileName) override
    {
        if (std::same_as<R, void>)
        {
            visitor_(fileName);
            return {};
        }
        else
        {
            return visitor_(fileName);
        }
    }
};
}

/** Visit each file in a directory.
*/
template<class Visitor>
Expected<void>
forEachFile(
    std::string_view dirPath,
    bool recursive,
    Visitor&& visitor)
{
    detail::FileVisitor<Visitor> v{visitor};
    return forEachFile(dirPath, recursive,
        static_cast<AnyFileVisitor&>(v));
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
Expected<void>
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

    @li Separators are replaced with the native separator

    @return The normalized path.

    @param pathName The relative or absolute path.
*/
MRDOCS_DECL
std::string
normalizePath(
    std::string_view pathName);

/** Return a normalized directory.

    This function returns a new directory path based on
    applying the changes defined by @ref normalizePath
    and @ref makeDirsy.

    @return The normalized path.

    @param pathName The relative or absolute path.
*/
MRDOCS_DECL
std::string
normalizeDir(
    std::string_view pathName);

/** Return the parent directory.

    If the parent directory is defined, the returned
    path will always have a trailing separator.
*/
MRDOCS_DECL
std::string
getParentDir(
    std::string_view pathName);

/** Return the parent directory.

    If the parent directory is defined, the returned
    path will always have a trailing separator.
*/
MRDOCS_DECL
std::string
getParentDir(
    std::string_view pathName,
    unsigned levels);

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

/** Check if the path is posix style.
*/
MRDOCS_DECL
bool
isPosixStyle(std::string_view pathName);

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

MRDOCS_DECL
std::string
appendPath(
    std::string_view basePath,
    std::string_view name1,
    std::string_view name2,
    std::string_view name3,
    std::string_view name4);

/** Return an error if the path is not a directory.
*/
MRDOCS_DECL
Expected<void>
requireDirectory(
    std::string_view pathName);

/** Determine if a path is a directory.
*/
MRDOCS_DECL
bool
isDirectory(
    std::string_view pathName);

/** Determine lexically if a path is a directory.

    This function determines if a path is a directory.

    If the path does not exist, the function
    determines lexically if the path represents
    a directory. In this case, the function
    returns true if the last path segment
    contains a period, otherwise false.
*/
MRDOCS_DECL
bool
isLexicalDirectory(
    std::string_view pathName);

/** Determine if a path is a directory.
*/
MRDOCS_DECL

bool
exists(
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
Expected<void>
createDirectory(
    std::string_view pathName);

/** Checks if the given path starts with the specified prefix.

    This function compares the beginning of the `pathName` with the `prefix`.
    It returns true if `pathName` starts with `prefix`. The comparison is case-sensitive.

    Unlike a direct string comparison, this function also accepts differences in the slashes used to separate paths.
    Therefore, it returns true even when the slashes used in `pathName` and `prefix` are not the same.
    The function accepts both forward slashes ("/") and backslashes ("\").

    @param pathName A string view representing the path to be checked.
    @param prefix A string view representing the prefix to be checked against the path.
    @return A boolean value. Returns true if `pathName` starts with `prefix`, false otherwise.
 */
MRDOCS_DECL
bool
startsWith(
    std::string_view pathName,
    std::string_view prefix);
} // files

} // mrdocs
} // clang

#endif