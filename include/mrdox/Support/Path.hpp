//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_SUPPORT_PATH_HPP
#define MRDOX_SUPPORT_PATH_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Support/Error.hpp>
#include <mrdox/Support/Expected.hpp>
#include <string>
#include <string_view>

namespace clang {
namespace mrdox {

//------------------------------------------------

struct AnyFileVisitor
{
    virtual Error visitFile(std::string_view fileName) = 0;
};

/** Call a function for each file in a directory.

    This will iterate all the regular files in
    a directory and invoke the visitor with the
    path.
*/
MRDOX_DECL
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

/** Return true if pathName is absolute.
*/
MRDOX_DECL
bool
isAbsolute(
    std::string_view pathName) noexcept;

/** Return true if pathName ends in a separator.
*/
MRDOX_DECL
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
MRDOX_DECL
std::string
normalizePath(
    std::string_view pathName);

/** Return the parent directory.

    If the parent directory is defined, the returned
    path will always have a trailing separator.
*/
MRDOX_DECL
std::string
getParentDir(
    std::string_view pathName);

/** Return the filename part of the path.
*/
MRDOX_DECL
std::string_view
getFileName(
    std::string_view pathName);

/** Return the contents of a file as a string.
*/
MRDOX_DECL
Expected<std::string>
getFileText(
    std::string_view pathName);

/** Append a trailing native separator if not already present.
*/
MRDOX_DECL
std::string
makeDirsy(
    std::string_view pathName);

/** Return an absolute path from a possibly relative path.

    Relative paths are resolved against the
    current working directory of the process.

    @return The absolute path, or an error if
    any occurred.
*/
MRDOX_DECL
Expected<std::string>
makeAbsolute(
    std::string_view pathName);

/** Return an absolute path from a possibly relative path.
*/
MRDOX_DECL
std::string
makeAbsolute(
    std::string_view pathName,
    std::string_view workingDir);

/** Convert all backward slashes to forward slashes.
*/
MRDOX_DECL
std::string
makePosixStyle(
    std::string_view pathName);

MRDOX_DECL
std::string
appendPath(
    std::string_view basePath,
    std::string_view pathName);

} // files

} // mrdox
} // clang

#endif