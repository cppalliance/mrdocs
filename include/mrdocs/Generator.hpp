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
#include <mrdocs/Support/Error/Error.hpp>
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

        The generator reads its configuration from the `config` passed in
        (a corpus does not own its configuration), resolves its own output
        location, and writes whatever files it needs.

        @par Thread Safety
        @li Different `corpus` object: may be called concurrently.
        @li Same `corpus` object: may not be called concurrently.

        @return The error, if any occurred.

        @param corpus The symbols to emit.
        @param config The configuration that drove the build.
    */
    MRDOCS_DECL
    virtual
    Expected<void>
    build(Corpus const& corpus, Config const& config) const = 0;
};

/** Install a custom generator.

    This function registers a generator with the global
    generator registry, making it available for use.

    A plugin installs its generators through
    @ref PluginContext::installGenerator, which calls
    this function.

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

/** Handlebars-based generators and the pieces that support them.

    The `hbs` namespace groups the operations tied to MrDocs's Handlebars
    output path, including the discovery of data-driven generators an addon
    contributes as manifest directories.
*/
namespace hbs {

/** Discover addon-defined data-driven generators and install them.

    For each configured addon root, the immediate subdirectories of
    `<root>/generator/` that ship an `mrdocs-generator.yml` manifest are
    installed into the global registry as data-driven Handlebars
    generators (see the manifest format documentation). A built-in
    generator of the same id takes precedence, so its addon directory is
    skipped. Directories that hold only shared assets and declare no
    manifest are skipped too.

    Call this once, after the configuration is resolved and before a
    generator is looked up by id with @ref findGenerator. It is one of the
    pieces the command-line tool composes to run its generate step; the
    order of that step lives in the tool.

    @par Thread Safety
    Installs into the process-global registry, so it may not be called
    concurrently with @ref installGenerator.

    @return The error, if any occurred.

    @param config The resolved configuration whose addon roots are walked.
*/
MRDOCS_DECL
Expected<void>
discoverDataDrivenGenerators(Config const& config);

} // hbs

} // mrdocs


#endif // MRDOCS_API_GENERATOR_HPP
