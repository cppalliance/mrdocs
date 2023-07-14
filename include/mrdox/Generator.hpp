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

#ifndef MRDOX_API_GENERATOR_HPP
#define MRDOX_API_GENERATOR_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Config.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/Support/Error.hpp>
#include <ostream>
#include <string>
#include <string_view>

namespace clang {
namespace mrdox {

/** Base class for documentation generators.
*/
class MRDOX_VISIBLE
    Generator
{
public:
    /** Destructor.
    */
    MRDOX_DECL
    virtual
    ~Generator() noexcept;

    /** Return the symbolic name of the generator.

        This is a short, unique string which identifies
        the generator in command line options and in
        configuration files.
    */
    MRDOX_DECL
    virtual
    std::string_view
    id() const noexcept = 0;

    /** Return the display name of the generator.
    */
    MRDOX_DECL
    virtual
    std::string_view
    displayName() const noexcept = 0;

    /** Return the extension or tag of the generator.

        This should be in all lower case. Examples
        of tags are:

        @li "adoc" Asciidoctor
        @li "xml" XML
        @li "html" HTML

        The returned string should not include
        a leading period.
    */
    MRDOX_DECL
    virtual
    std::string_view
    fileExtension() const noexcept = 0;

    /** Build reference documentation for the corpus.

        This function invokes the generator to emit
        the documentation using @ref outputPath as
        a parameter indicating where the output should
        go. This can be a directory or a filename
        depending on the generator and how it is
        configured.

        @return The error, if any occurred.

        @param outputPath An existing directory or
        a filename.

        @param corpus The symbols to emit. The
        generator may modify the contents of
        the object before returning.

        @param config The configuration to use.
    */
    MRDOX_DECL
    virtual
    Error
    build(
        std::string_view outputPath,
        Corpus const& corpus) const;

    /** Build reference documentation for the corpus.

        This function invokes the generator to emit
        the full documentation to an output stream,
        as a single entity.

        @par Thread Safety
        @li Different `corpus` object: may be called concurrently.
        @li Same `corpus` object: may not be called concurrently.

        @return The error, if any occurred.

        @param os The stream to write to.

        @param corpus The metadata to emit.
    */
    MRDOX_DECL
    virtual
    Error
    buildOne(
        std::ostream& os,
        Corpus const& corpus) const = 0;

    /** Build the reference as a single page to a file.

        @par Thread Safety
        @li Different `corpus` object: may be called concurrently.
        @li Same `corpus` object: may not be called concurrently.

        @return The error, if any occurred.

        @param fileName The file to write. If the
        file already exists, it will be overwritten.

        @param corpus The metadata to emit.
    */
    MRDOX_DECL
    Error
    buildOne(
        std::string_view fileName,
        Corpus const& corpus) const;

    /** Build the reference as a single page to a string.

        @return The error, if any occurred.

        @param dest The string to hold the result.
        For the duration of the call, this must
        not be accessed by any other threads.

        @param corpus The metadata to emit.
    */
    MRDOX_DECL
    Error
    buildOneString(
        std::string& dest,
        Corpus const& corpus) const;
};

} // mrdox
} // clang

#endif
