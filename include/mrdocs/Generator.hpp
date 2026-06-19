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

// Generator classes for converting declaration
// information into documentation in a specified format.

#ifndef MRDOCS_API_GENERATOR_HPP
#define MRDOCS_API_GENERATOR_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Support/Error.hpp>
#include <memory>
#include <string>
#include <string_view>


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

    /** Build the documentation for the corpus.

        The generator reads its configuration from `corpus.config`,
        resolves its own output location, and writes whatever files it
        needs.

        @par Thread Safety
        @li Different `corpus` object: may be called concurrently.
        @li Same `corpus` object: may not be called concurrently.

        @return The error, if any occurred.

        @param corpus The symbols to emit.
    */
    MRDOCS_DECL
    virtual
    Expected<void>
    build(Corpus const& corpus) const = 0;
};

/** Install a custom generator.

    This function registers a generator with the global
    generator registry, making it available for use.

    Plugins can use this function to register custom
    generators.

    @par Thread Safety
    This function is thread-safe and may be called
    concurrently from multiple threads.

    @return An error if a generator with the same id
    already exists.

    @param G The generator to install. Ownership is
    transferred to the registry.
*/
MRDOCS_DECL
Expected<void>
installGenerator(std::unique_ptr<Generator> G);

/** Find a generator by its id.

    @par Thread Safety
    This function is thread-safe and may be called
    concurrently from multiple threads.

    @return A pointer to the generator, or `nullptr`
    if no generator with the given id exists.

    @param id The symbolic name of the generator.
    The name must be an exact match, including case.
*/
MRDOCS_DECL
Generator const*
findGenerator(std::string_view id) noexcept;

} // mrdocs


#endif // MRDOCS_API_GENERATOR_HPP
