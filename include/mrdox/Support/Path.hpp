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

/** Append a trailing native separator if not already present.
*/
MRDOX_DECL
std::string_view
makeDirsy(
    std::string& dirName);

/** Return a native absolute path representing a path.

    This function returns an absolute path using
    native separators given a relative or absolute
    path.

    If the input path is relative, it is first made
    absolute by resolving it against the configuration's
    working directory.

    The input path can use native, POSIX, or Windows
    separators.

    The returned path will have a trailing separator.
*/
MRDOX_DECL
std::string
makeAbsoluteDirectory(
    std::string_view dirName,
    std::string_view workingDir);

MRDOX_DECL
std::string
makeFilePath(
    std::string_view dirName,
    std::string_view fileName);

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

MRDOX_DECL
std::string
appendPath(
    std::string_view basePath,
    std::string_view pathName);

} // files

} // mrdox
} // clang

#endif