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
#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <memory>
#include <string>
#include <string_view>


namespace mrdocs {

class GeneratorRegistry;

/** Base class for documentation generators.

    The generators a run can select live in a @ref GeneratorRegistry,
    which owns them and finds them by id. A plugin installs its generators
    through @ref PluginContext::registerGenerator, and a program adds its
    own with @ref GeneratorRegistry::install.
*/
class MRDOCS_DECL
    Generator
{
public:
    /** Destructor.
    */
    virtual
    ~Generator() noexcept;

    /** Return the symbolic name of the generator.

        This is a short, unique string which identifies
        the generator in command line options and in
        configuration files.
    */
    virtual
    std::string_view
    id() const noexcept = 0;

    /** Return the display name of the generator.
    */
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
    virtual
    std::string_view
    fileExtension() const noexcept = 0;

    /** Build the documentation for the corpus.

        A generator reports through `reporter`, the reporter of the session
        that runs it.

        The generator reads its configuration from the `config` passed in
        (a corpus does not own its configuration) and its own options with
        @ref getGeneratorParams, resolves its own output location, and writes
        whatever files it needs.

        @par Thread Safety
        @li Different `corpus` object: may be called concurrently.
        @li Same `corpus` object: may not be called concurrently.

        @return The error, if any occurred.

        @param corpus The symbols to emit.
        @param config The configuration that drove the build.
        @param reporter The reporter of the run.
    */
    virtual Expected<void>
    build(
        Corpus const& corpus,
        Config const& config,
        report::Reporter& reporter) const = 0;
};

/** Resolve the output location for a generator from the configuration.

    A per-generator `output` (under `generator-options.<id>`) is used as
    given. Otherwise the top-level `output` is used directly for a single
    generator, or a subdirectory named after the generator id when several
    run, so each writes into its own directory. All paths are resolved
    relative to the configuration file.

    Generators call this from @ref Generator::build to find where their
    files go; they remain free to interpret the result as a file or a
    directory.

    @return The absolute output path for the generator.

    @param generator The generator whose output is being resolved.
    @param config The configuration that drives the result.
*/
MRDOCS_DECL
std::string
getGeneratorOutputPath(Generator const& generator, Config const& config);

/** Return the options block of a generator.

    This is the `generator-options.<id>` block of the configuration for the
    generator's @ref Generator::id, or an empty object when the configuration
    has none. It is the one place that reads `Config::generatorOptions` for a
    generator: a generator class calls it from @ref Generator::build, and the
    wrappers of callable and script generators fill
    @ref GeneratorExtensionContext::params with it.

    @return The block, which shares its storage with the configuration.

    @param generator The generator whose block is looked up.
    @param config The configuration to read the block from.
*/
MRDOCS_DECL
dom::Object
getGeneratorParams(Generator const& generator, Config const& config);

/** Handlebars-based generators and the pieces that support them.

    The `hbs` namespace groups the operations tied to MrDocs's Handlebars
    output path, including the discovery of data-driven generators an addon
    contributes as manifest directories.
*/
namespace hbs {

/** Discover addon-defined data-driven generators and add them.

    For each configured addon root, the immediate subdirectories of
    `<root>/generator/` that ship an `mrdocs-generator.yml` manifest are
    added to the registry as data-driven Handlebars generators (see the
    manifest format documentation). A generator of the same id that the
    registry already holds takes precedence, so its addon directory is
    skipped; so is one an earlier addon root contributed. Directories that
    hold only shared assets and declare no manifest are skipped too.

    Call this once per configuration, after the extensions are loaded and
    before a generator is looked up by id with
    @ref GeneratorRegistry::find. It is one of the pieces
    @ref Session::create composes: it runs after the extensions are loaded,
    so a generator an extension installs takes precedence over an addon
    directory of the same id. A program that wants that order constructs a
    @ref Session.

    @par Thread Safety
    The call is synchronized with the other calls that add or look up a
    generator of `registry`.

    @return The error, if any occurred.

    @param config The resolved configuration whose addon roots are walked.
    @param registry The registry the generators are added to.
*/
MRDOCS_DECL
Expected<void>
discoverDataDrivenGenerators(
    Config const& config,
    GeneratorRegistry& registry);

} // hbs

} // mrdocs


#endif // MRDOCS_API_GENERATOR_HPP
