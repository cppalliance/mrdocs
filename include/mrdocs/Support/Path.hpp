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

    @param dirPath The path to the directory.
    @param recursive If true, files in subdirectories are
    also visited, recursively.
    @param visitor The visitor to invoke for each file.
    @return An error if any occurred.
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

    @param dirPath The path to the directory.
    @param recursive If true, files in subdirectories are
        also visited, recursively.
    @param visitor A callable object which is invoked
        for each file.
    @return An error if any occurred.

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

/** The type of a file.
*/
enum class FileType
{
    /// The file does not exist
    not_found,
    /// The path represents a regular file
    regular,
    /// The file is a directory
    directory,
    /// The file is something else
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

    @param pathName The absolute or relative path
        to the directory or file.
    @return true if the path is absolute,
        false otherwise.
*/
MRDOCS_DECL
bool
isAbsolute(
    std::string_view pathName) noexcept;

/** Return an error if pathName is not absolute.

    @param pathName The absolute or relative path
        to the directory or file.
    @return An error if the path is not absolute.
*/
MRDOCS_DECL
Expected<void>
requireAbsolute(
    std::string_view pathName);

/** Return true if pathName ends in a separator.

    @param pathName The absolute or relative path
        to the directory or file.
    @return true if the path ends in a separator,
        false otherwise.
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

    @param pathName The absolute or relative path
        to the directory or file.
    @return The parent directory, or the empty
        string if there is none.
*/
MRDOCS_DECL
std::string
getParentDir(
    std::string_view pathName);

/** Return the parent directory.

    If the parent directory is defined, the returned
    path will always have a trailing separator.

    @param pathName The absolute or relative path
        to the directory or file.
    @param levels The number of levels to go up.
        If this is zero, the original path is returned.
        If this is greater than the number of levels
        in the path, the empty string is returned.
    @return The parent directory, or the empty
        string if there is none.
*/
MRDOCS_DECL
std::string
getParentDir(
    std::string_view pathName,
    unsigned levels);

/** Return the filename part of the path.

    @param pathName The absolute or relative path
        to the directory or file.
    @return The filename part of the path,
        or the empty string if there is none.
*/
MRDOCS_DECL
std::string_view
getFileName(
    std::string_view pathName);

/** Return the contents of a file as a string.

    @param pathName The absolute or relative path
        to the file.
    @return The contents of the file, or an error
        if any occurred.
*/
MRDOCS_DECL
Expected<std::string>
getFileText(
    std::string_view pathName);

/** Append a trailing native separator if not already present.

    @param pathName The absolute or relative path
        to the directory or file.
    @return A copy of the path with a trailing
        separator if not already present.
*/
MRDOCS_DECL
std::string
makeDirsy(
    std::string_view pathName);

/** Return an absolute path from a possibly relative path.

    Relative paths are resolved against the
    current working directory of the process.

    @param pathName The absolute or relative path
        to the directory or file.
    @return The absolute path, or an error if
    any occurred.
*/
MRDOCS_DECL
Expected<std::string>
makeAbsolute(
    std::string_view pathName);

/** Return an absolute path from a possibly relative path.

    @param pathName The absolute or relative path
        to the directory or file.
    @param workingDir The working directory to
        resolve relative paths against.
    @return The absolute path, or an error if
        any occurred.
*/
MRDOCS_DECL
std::string
makeAbsolute(
    std::string_view pathName,
    std::string_view workingDir);

/** Convert all backward slashes to forward slashes.

    @param pathName The absolute or relative path
        to the directory or file.
    @return A copy of the path with all
        backslashes replaced with forward slashes.
*/
MRDOCS_DECL
std::string
makePosixStyle(
    std::string_view pathName);

/** Check if the path is posix style.

    @param pathName The absolute or relative path
        to the directory or file.
    @return true if the path uses only forward slashes
        as path separators, false otherwise.
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

    @param pathName The absolute or relative path
    @return An error if the path does not exist
            or is not a directory.
*/
MRDOCS_DECL
Expected<void>
requireDirectory(
    std::string_view pathName);

/** Determine if a path is a directory.

    @param pathName The absolute or relative path
    @return true if the path exists and is a directory,
        false otherwise.
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

    @param pathName The absolute or relative path
    @return true if the path exists and is a directory,
        or if the path does not exist and the last path
        segment does not contain a period.
        false otherwise.
*/
MRDOCS_DECL
bool
isLexicalDirectory(
    std::string_view pathName);

/** Determine if a path exists

    @param pathName The absolute or relative path
    @return true if the path exists, false otherwise.
*/
MRDOCS_DECL
bool
exists(
    std::string_view pathName);

/** Return the relevant suffix of a source file path.

    @param pathName The absolute or relative path
    to the file.
    @return The suffix, including the leading dot,
    or the empty string if there is no suffix.
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