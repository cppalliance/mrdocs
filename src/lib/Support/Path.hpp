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

#ifndef MRDOCS_LIB_SUPPORT_PATH_HPP
#define MRDOCS_LIB_SUPPORT_PATH_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Support/Path.hpp>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Path.h>

namespace clang {
namespace mrdocs {

/** A reasonably sized small string for paths.

    This is for local variables not for use
    as data members of long-lived types.
*/
using SmallPathString = llvm::SmallString<340>;

/** Replaces backslashes with slashes if Windows in place.

    @param path A path that is transformed to native format.

    On Unix, this function is a no-op because backslashes
    are valid path chracters.
*/
llvm::StringRef
convert_to_slash(
    llvm::SmallVectorImpl<char> &path,
    llvm::sys::path::Style style =
        llvm::sys::path::Style::native);

/** A temporary file that is deleted when it goes out of scope.
*/
class ScopedTempFile
{
    clang::mrdocs::SmallPathString path_;
    bool ok_ = false;
public:
    /** Destructor

        If a file was created, it is deleted.

     */
    ~ScopedTempFile();

    /** Constructor

        Creates a temporary file with the given prefix and extension.
        The file is deleted when this object goes out of scope.

        The file is created in the system temporary directory with
        the following format:

        @code
        <tempdir>/<prefix><random>.<ext>
        @endcode

        @param prefix The prefix for the temporary file name.
        @param ext The extension for the temporary file name.
    */
    ScopedTempFile(llvm::StringRef prefix, llvm::StringRef ext);

    /** Returns `true` if the file was created successfully.
    */
    operator bool() const { return ok_; }

    /** Returns the path to the temporary file.
     */
    llvm::StringRef path() const { return path_; }
};

/** A temporary directory that is deleted when it goes out of scope.
*/
class ScopedTempDirectory
{
    clang::mrdocs::SmallPathString path_;
    bool ok_ = false;
public:
    /** Destructor

        If a directory was created, it is deleted.

     */
    ~ScopedTempDirectory();

    /** Constructor

        Creates a temporary directory with the given prefix.
        The directory is deleted when this object goes out of scope.

        The directory is created in the system temporary directory with
        the following format:

        @code
        <tempdir>/<prefix><random>
        @endcode

        @param prefix The prefix for the temporary directory name.
    */
    ScopedTempDirectory(llvm::StringRef prefix);

    /** Returns `true` if the directory was created successfully.
    */
    operator bool() const { return ok_; }

    /** Returns the path to the temporary directory.
     */
    llvm::StringRef path() const { return path_; }
};

} // mrdocs
} // clang

#endif
