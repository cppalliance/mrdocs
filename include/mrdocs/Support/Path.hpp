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


namespace mrdocs {

//------------------------------------------------

/** Polymorphic visitor for files discovered during traversal.
*/
struct MRDOCS_VISIBLE
    AnyFileVisitor
{
    /** Virtual destructor.
    */
    virtual
    ~AnyFileVisitor() = 0;

    /** Visit a single file path.
        @param fileName Path to the file being visited.
        @return Success or error from visitor.
    */
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
        for each file. This visitor might return
        `void` or `Expected<void>`.
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

/** Filesystem helpers (join, temp, real-path resolution) used throughout MrDocs.

    The `files` namespace centralizes cross-platform path manipulation so CLI,
    generators, and tests can share the same normalization and staging logic.
*/
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

    The result is a view into `pathName` (like @ref getFileName), so it is
    valid only while `pathName` is. Wrap it in a `std::string` to keep it.

    @param pathName The absolute or relative path
        to the directory or file.
    @return The parent directory, or the empty
        string if there is none.
*/
MRDOCS_DECL
std::string_view
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

/** Return the real, symlink-resolved form of a path.

    Resolves every symbolic link and `.`/`..` component in `pathName`
    to the file's actual on-disk location and returns it in POSIX
    style (forward slashes). The same file can be named by many paths
    (through symlinks, `..`, or a directory that is itself a link);
    this collapses all of those spellings to one real path, so two
    paths that refer to the same file produce the same result.

    If the path cannot be resolved, for example because it does not
    exist on disk, is a virtual or in-memory file, or has not been
    created yet, the input is returned unchanged apart from POSIX
    normalization. Callers can therefore always fall back to comparing
    the path as written, and this function never fails.

    @param pathName The absolute or relative path to a directory or file.
    @return The real POSIX path, or the POSIX-normalized input if it
        cannot be resolved.
*/
MRDOCS_DECL
std::string
makeRealPath(std::string_view pathName);

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

/** Append a component to a base path using the native separator.

    @param basePath Base path.
    @param name Component to append.
    @return Combined path string.
*/
MRDOCS_DECL
std::string
appendPath(
    std::string_view basePath,
    std::string_view name);

/** Append two components to a base path using the native separator.

    @param basePath Base path.
    @param name1 First component to append.
    @param name2 Second component to append.
    @return Combined path string.
*/
MRDOCS_DECL
std::string
appendPath(
    std::string_view basePath,
    std::string_view name1,
    std::string_view name2);

/** Append three components to a base path using the native separator.

    @param basePath Base path.
    @param name1 First component to append.
    @param name2 Second component to append.
    @param name3 Third component to append.
    @return Combined path string.
*/
MRDOCS_DECL
std::string
appendPath(
    std::string_view basePath,
    std::string_view name1,
    std::string_view name2,
    std::string_view name3);

/** Append four components to a base path using the native separator.

    @param basePath Base path.
    @param name1 First component to append.
    @param name2 Second component to append.
    @param name3 Third component to append.
    @param name4 Fourth component to append.
    @return Combined path string.
*/
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

/** Determine if a path is a regular file.

    Symbolic links are followed, so a link to a regular file counts as a
    regular file.

    @param pathName The absolute or relative path
    @return true if the path exists and is a regular file,
        false otherwise.
*/
MRDOCS_DECL
bool
isRegularFile(
    std::string_view pathName);

/** Determine whether a path looks like a directory.

    When the path exists, this reports whether it is a directory.
    When it does not exist yet, it falls back to a lexical hint: the
    path looks like a directory when its last segment has no extension
    (contains no period), and like a file otherwise.

    This is useful for output paths, which often do not exist when a
    generator is deciding whether to treat the path as a file or a
    directory.

    @param pathName The absolute or relative path
    @return true if the path exists and is a directory, or does not
        exist and its last segment contains no period; false otherwise.
*/
MRDOCS_DECL
bool
looksLikeDirectory(
    std::string_view pathName);

/** Determine whether a path looks like a file.

    The inverse of @ref looksLikeDirectory: when the path exists this
    reports whether it is not a directory; when it does not exist yet
    the path looks like a file when its last segment has an extension
    (contains a period).

    @param pathName The absolute or relative path
    @return true when @ref looksLikeDirectory would return false.
*/
MRDOCS_DECL
bool
looksLikeFile(
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

/** Checks whether the given path lies within the specified prefix.

    It returns true if `pathName` is `prefix` or lies underneath it. The
    comparison walks whole path segments rather than characters, so `/abc`
    contains `/abc/def` but not `/abcdef`. The comparison is case-sensitive.

    Unlike a direct string comparison, this function also accepts differences in the slashes used to separate paths.
    Therefore, it returns true even when the slashes used in `pathName` and `prefix` are not the same.
    The function accepts both forward slashes ("/") and backslashes ("\").

    It does not resolve symbolic links; use @ref isResolvedSubpathOf when
    symlink equivalence matters.

    @param pathName A string view representing the path to be checked.
    @param prefix A string view representing the prefix to be checked against the path.
    @return A boolean value. Returns true if `pathName` lies within `prefix`, false otherwise.
*/
MRDOCS_DECL
bool
isSubpathOf(
    std::string_view pathName,
    std::string_view prefix);

/** Checks whether the given path lies within the specified prefix, tolerating symlinks.

    This behaves like @ref isSubpathOf, but also returns true when `pathName`
    lies within `prefix` only after resolving symbolic links on both sides
    (see @ref makeRealPath). It recognizes a file reached through a symlinked
    directory while `prefix` refers to the real path, or vice versa.

    Matching is generous: a lexical match alone is enough, and resolving
    symlinks only ever adds matches, so anything @ref isSubpathOf accepts is
    still accepted. This makes it suitable for inclusion decisions. Prefer
    the strict @ref isSubpathOf for exclusion decisions, so that a symlinked
    alias of an excluded file is not excluded.

    The symlink resolution performs filesystem lookups, so it is reached only
    when the lexical comparison fails; paths with no symlinks pay no extra cost.

    @param pathName A string view representing the path to be checked.
    @param prefix A string view representing the prefix to be checked against the path.
    @return A boolean value. Returns true if `pathName` lies within `prefix` as written or after resolving symlinks, false otherwise.
*/
MRDOCS_DECL
bool
isResolvedSubpathOf(
    std::string_view pathName,
    std::string_view prefix);
} // files

} // mrdocs


#endif
