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

    /** Build documentation from the corpus and configuration.

        The default implementation calls @ref buildOneFile
        with a suitable filename calculated from the
        root directory and the generator's extension.

        @par Thread Safety
        @li Different `corpus` object: may be called concurrently.
        @li Same `corpus` object: may not be called concurrently.

        @return `true` upon success.

        @par outputPath The path to a directory for
        emitting multi-file output, or the path of
        a filename with the generator's extension
        to emit output as a single file.

        @par corpus The symbols to emit. The
        generator may modify the contents of
        the object before returning.

        @par config The configuration to use.

        @par R The diagnostic reporting object to
        use for delivering errors and information.
    */
    virtual
    bool
    build(
        llvm::StringRef outputPath,
        Corpus& corpus,
        Config const& config,
        Reporter& R) const;

    /** Build single-file documentation from the corpus and configuration.

        @par Thread Safety
        @li Different `corpus` object: may be called concurrently.
        @li Same `corpus` object: may not be called concurrently.

        @return `true` upon success.

        @par fileName The path to the file
        which will be created or reset to hold
        the generated results.

        @par corpus The symbols to emit. The
        generator may modify the contents of
        the object before returning.

        @par config The configuration to use.

        @par R The diagnostic reporting object to
        use for delivering errors and information.
    */
    virtual
    bool
    buildOne(
        llvm::StringRef fileName,
        Corpus& corpus,
        Config const& config,
        Reporter& R) const = 0;

    /** Build a string containing the single-file documentation.

        @par Thread Safety
        @li Different `corpus` object: may be called concurrently.
        @li Same `corpus` object: may not be called concurrently.

        @return `true` upon success.

        @par dest The string to hold the result.
        For the duration of the call, this must
        not be accessed by any other threads.

        @par corpus The symbols to emit. The
        generator may modify the contents of
        the object before returning.

        @par config The configuration to use.

        @par R The diagnostic reporting object to
        use for delivering errors and information.
    */
    virtual
    bool
    buildString(
        std::string& dest,
        Corpus& corpus,
        Config const& config,
        Reporter& R) const = 0;
};

extern std::unique_ptr<Generator> makeXMLGenerator();
extern std::unique_ptr<Generator> makeAsciidocGenerator();

} // mrdox
} // clang

#endif
