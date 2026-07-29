//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_FILESYSTEM_TEMP_HPP
#define MRDOCS_LIB_SUPPORT_FILESYSTEM_TEMP_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Path.h>
#include <string>
#include <string_view>
#include <vector>


namespace mrdocs {

/** A reasonably sized small string for paths.

    This is for local variables not for use
    as data members of long-lived types.
*/
using SmallPathString = llvm::SmallString<340>;

/** A temporary file that is deleted when it goes out of scope.
*/
class ScopedTempFile
{
    mrdocs::SmallPathString path_;
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
    // Status of the directory
    enum class ErrorStatus
    {
        None,
        CannotDeleteExisting,
        CannotCreateDirectories
    };

    mrdocs::SmallPathString path_;
    ErrorStatus status_ = ErrorStatus::None;
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

        For instance, if the prefix is "mrdocs" and the operating system
        is Unix, the directory might be created as: "/tmp/mrdocs-1234".

        On Windows, the directory might be created as:
        "C:\Users\user\AppData\Local\Temp\mrdocs-1234".

        @param prefix The prefix for the temporary directory name.
    */
    ScopedTempDirectory(llvm::StringRef prefix);

    /** Constructor with a specific path

        Creates a temporary directory with the given path.
        The directory is deleted when this object goes out of scope.

        @param root The root directory for the temporary directory.
        @param dir The name of the temporary directory.
    */
    ScopedTempDirectory(llvm::StringRef root, llvm::StringRef dir);

    /** Move ownership of the created directory.

        The moved-from object is left empty so it does not delete the
        directory; the destination owns and eventually removes it.
    */
    ScopedTempDirectory(ScopedTempDirectory&& other) noexcept;
    ScopedTempDirectory& operator=(ScopedTempDirectory&& other) noexcept;

    ScopedTempDirectory(ScopedTempDirectory const&) = delete;
    ScopedTempDirectory& operator=(ScopedTempDirectory const&) = delete;

    /** Returns `true` if the directory was created successfully.
    */
    operator bool() const
    {
        return status_ == ErrorStatus::None;
    }

    /** Returns `true` if the directory was not created successfully.
    */
    bool
    failed() const
    {
        return status_ != ErrorStatus::None;
    }

    /** Returns the path to the temporary directory.
    */
    std::string_view path() const { return static_cast<llvm::StringRef>(path_); }

    /** Returns the error status of the directory.
    */
    Error error() const;

    /** Convert temp directory to a std::string_view
    */
    operator std::string_view() const { return path(); }
};

/** Create a scratch directory in the first writable candidate location.

    MrDocs needs a writable place for transient build artifacts (for
    example the CMake build tree used to generate a compilation database).
    Candidates are tried in order until one exists (or can be created) and
    is writable, and a uniquely-named subdirectory is created inside it:

    @li the directory named by the `MRDOCS_TEMP_DIR` environment variable;
    @li the MrDocs user cache directory (`<os-cache>/mrdocs`);
    @li the system temporary directory (which also honors `TMPDIR`,
        `TMP`, and `TEMP`);
    @li the caller-supplied `fallbacks`, in order, used last.

    The returned directory is removed when the result goes out of scope.

    @return The scratch directory, or an error if no candidate qualified.

    @param subject A short label folded into the directory name so the
    scratch directory is identifiable and unique.
    @param fallbacks Extra base directories to try after the defaults,
    such as the output or configuration directory.
*/
MRDOCS_DECL
Expected<ScopedTempDirectory>
makeScratchDirectory(
    std::string_view subject,
    std::vector<std::string> const& fallbacks = {});

} // mrdocs


#endif // MRDOCS_LIB_SUPPORT_FILESYSTEM_TEMP_HPP
