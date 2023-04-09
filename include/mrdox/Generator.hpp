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
#include <mrdox/Errors.hpp>
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

        @return `true` upon success.
    */
    virtual
    bool
    build(
        llvm::StringRef rootPath,
        Corpus const& corpus,
        Config const& config,
        Reporter& R) const;

    /** Build single-file documentation from the corpus and configuration.

        @return `true` upon success.
    */
    virtual
    bool
    buildOne(
        llvm::StringRef fileName,
        Corpus const& corpus,
        Config const& config,
        Reporter& R) const = 0;

    /** Build a string containing the single-file documentation.
    */
    virtual
    bool
    buildString(
        std::string& dest,
        Corpus const& corpus,
        Config const& config,
        Reporter& R) const = 0;
};

extern std::unique_ptr<Generator> makeXMLGenerator();
extern std::unique_ptr<Generator> makeAsciidocGenerator();

//------------------------------------------------

// VFALCO This does not belong here
std::string getTagType(TagTypeKind AS);

} // mrdox
} // clang

#endif
