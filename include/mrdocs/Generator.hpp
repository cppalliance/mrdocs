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

// Generator classes for converting declaration
// information into documentation in a specified format.

#ifndef MRDOCS_API_GENERATOR_HPP
#define MRDOCS_API_GENERATOR_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Support/Error.hpp>
#include <ostream>
#include <string>
#include <string_view>

namespace clang {
namespace mrdocs {

/** Base class for documentation generators.
*/
class MRDOCS_VISIBLE
    Generator
{
public:
    /** Destructor.
    */
    MRDOCS_DECL
    virtual
    ~Generator() noexcept;

    /** Return the symbolic name of the generator.

        This is a short, unique string which identifies
        the generator in command line options and in
        configuration files.
    */
    MRDOCS_DECL
    virtual
    std::string_view
    id() const noexcept = 0;

    /** Return the display name of the generator.
    */
    MRDOCS_DECL
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
    MRDOCS_DECL
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

        The default implentation assumes the output
        is single-page and emits the file `reference.ext`
        using @ref buildOne to generate the content.

        The typical implementation will have behavior
        similar to the default implementation if the
        output is single-page, or will iterate over
        the symbols in the corpus to generate multiple
        files if the output is multi-page.

        @return The error, if any occurred.

        @param outputPath A directory or filename.

        @param corpus The symbols to emit. The
        generator may modify the contents of
        the object before returning.
    */
    MRDOCS_DECL
    virtual
    Expected<void>
    build(
        std::string_view outputPath,
        Corpus const& corpus) const;

    /** Build reference documentation for the corpus.

        This function invokes the generator to emit
        the documentation using the corpus configuration
        options to determine the output location.

        @return The error, if any occurred.

        @param corpus The symbols to emit. The
        generator may modify the contents of
        the object before returning.
    */
    MRDOCS_DECL
    Expected<void>
    build(Corpus const& corpus) const;

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
    MRDOCS_DECL
    virtual
    Expected<void>
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
    MRDOCS_DECL
    Expected<void>
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
    MRDOCS_DECL
    Expected<void>
    buildOneString(
        std::string& dest,
        Corpus const& corpus) const;
};

} // mrdocs
} // clang

#endif
