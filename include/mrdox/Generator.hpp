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

// Generator classes for converting declaration
// information into documentation in a specified format.

#ifndef MRDOX_GENERATOR_HPP
#define MRDOX_GENERATOR_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Config.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/Reporter.hpp>
#include <llvm/ADT/StringRef.h>

namespace clang {
namespace mrdox {

/** Base class for documentation generators.
*/
struct Generator
{
    /** Destructor.
    */
    virtual ~Generator() = default;

    /** Return the full name of the generator.
    */
    virtual
    llvm::StringRef
    name() const noexcept = 0;

    /** Return the extension or tag of the generator.

        This should be in all lower case. Examples
        of tags are:

        @li "adoc" Asciidoctor
        @li "xml" XML

        The returned string should not include
        a leading period.
    */
    virtual
    llvm::StringRef
    extension() const noexcept = 0;

    /** Build multi-page documentation from the corpus and configuration.

        @par Thread Safety
        @li Different `corpus` object: may be called concurrently.
        @li Same `corpus` object: may not be called concurrently.

        @return The error, if any occurred.

        @param outputPath An existing directory or
        a filename.

        @param corpus The symbols to emit. The
        generator may modify the contents of
        the object before returning.

        @param config The configuration to use.

        @param R The diagnostic reporting object to
        use for delivering errors and information.
    */
    virtual
    llvm::Error
    buildPages(
        llvm::StringRef outputPath,
        Corpus const& corpus,
        Reporter& R) const;

    /** Build the reference as a single page to an output stream.

        @par Thread Safety
        @li Different `corpus` object: may be called concurrently.
        @li Same `corpus` object: may not be called concurrently.

        @return The error, if any occurred.

        @param os The stream to write to.

        @param corpus The metadata to emit.

        @param R The diagnostic reporting object to use.

        @param fd_os A pointer to an optional, file-based
            output stream. If this is not null, the function
            fd_os->error() will be called periodically and
            the result returend if an error is indicated.
    */
    virtual
    llvm::Error
    buildSinglePage(
        llvm::raw_ostream& os,
        Corpus const& corpus,
        Reporter& R,
        llvm::raw_fd_ostream* fd_os = nullptr) const = 0;

    /** Build the reference as a single page to a file.

        @par Thread Safety
        @li Different `corpus` object: may be called concurrently.
        @li Same `corpus` object: may not be called concurrently.

        @return The error, if any occurred.

        @param filePath The file to write. If the
        file already exists, it will be overwritten.

        @param corpus The metadata to emit.

        @param R The diagnostic reporting object to use.
    */
    llvm::Error
    buildSinglePageFile(
        llvm::StringRef filePath,
        Corpus const& corpus,
        Reporter& R) const;

    /** Build the reference as a single page to a string.

        @par Thread Safety
        @li Different `corpus` object: may be called concurrently.
        @li Same `corpus` object: may not be called concurrently.

        @return The error, if any occurred.

        @param dest The string to hold the result.
        For the duration of the call, this must
        not be accessed by any other threads.

        @param corpus The metadata to emit.

        @param R The diagnostic reporting object to use.
    */
    llvm::Error
    buildSinglePageString(
        std::string& dest,
        Corpus const& corpus,
        Reporter& R) const;
};

extern std::unique_ptr<Generator> makeXMLGenerator();
extern std::unique_ptr<Generator> makeAsciidocGenerator();

} // mrdox
} // clang

#endif
