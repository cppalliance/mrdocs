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

#ifndef MRDOCS_API_CONFIG_HPP
#define MRDOCS_API_CONFIG_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ConfigSchema.hpp>
#include <mrdocs/Dom/Object.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <mrdocs/Config/ReferenceDirectories.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace mrdocs {

/** Configuration used to generate the Corpus and Docs.

    Holds the public options (in their original data types) read from the
    command line and the YAML file, plus the DOM view of those options that
    templates consume. It is a plain value: copy or move it freely, and pass
    it by reference to the steps that need it.

    The configuration is connected to the directory of the mrdocs.yml file,
    from which absolute paths are resolved from relative paths. Reference
    directories (working directory, mrdocs root) are not stored here; they
    flow through the workflow as a separate @ref ReferenceDirectories.
*/
class MRDOCS_DECL
    Config : public ConfigSchema
{
public:
    /** Load the configuration options from a YAML string and command line.

        Populates the options from the YAML, then applies the `--option`
        overrides parsed from @p argv on top (a no-op when @p argv is null).
        Pass an empty @p configYaml to apply command-line values alone. Paths
        are resolved later by @ref normalize.

        @param c The configuration to populate.
        @param configYaml The configuration YAML (may be empty).
        @param argv A null-terminated command line, or null for none.
        @return Nothing on success, otherwise an error.
    */
    static
    Expected<void>
    load(
        Config& c,
        std::string_view configYaml,
        char const** argv = nullptr);

    /** Load the configuration options from a YAML file.

        @param c The configuration to populate.
        @param configPath The path to the configuration file.
        @return Nothing on success, otherwise an error.
    */
    static
    Expected<void>
    load_file(
        Config& c,
        std::string_view configPath);

    /** Load a config file, apply command-line overrides, and finalize it.

        The single entry point for the common case: it reads @p configPath,
        applies the `--option` overrides parsed from @p argv on top of it,
        normalizes and validates everything against @p dirs, restores the
        configured log level (startup forces it low so option parsing stays
        quiet), and reports any unrecognized keys. @p argv is a
        null-terminated array; use the overload without it when there is no
        command line to apply.

        @param c The configuration to populate.
        @param configPath The path to the configuration file.
        @param dirs The reference directories used to resolve relative paths.
        @param argv The command line whose `--option` values override the file.
        @return Nothing on success, otherwise an error.
    */
    static
    Expected<void>
    load_file(
        Config& c,
        std::string_view configPath,
        ReferenceDirectories const& dirs,
        char const** argv);

    /// @copydoc load_file(Config&, std::string_view, ReferenceDirectories const&, char const**)
    static
    Expected<void>
    load_file(
        Config& c,
        std::string_view configPath,
        ReferenceDirectories const& dirs);

    /** Normalize the options against the reference directories.

        Applies defaults, validates values, and resolves relative paths.
        Call this after any command-line overrides are applied.

        @param dirs The reference directories used to resolve relative paths.
        @return Nothing on success, otherwise an error.
    */
    Expected<void>
    normalize(ReferenceDirectories const& dirs);

    /** Full path to the config file directory.

        The reference directory for most MrDocs options is the directory of
        the mrdocs.yml file; it is used to resolve relative paths. The string
        is native style with a trailing directory separator.

        @return The full path to the config file directory.
    */
    std::string
    configDir() const;

    /** Warn about configuration keys that match no known option.

        Reports each key collected during load: as a warning, or as an
        error under `warn-as-error`, and does nothing when
        `warn-unknown-config-keys` is disabled. This is a separate,
        explicitly-called step rather than part of load because the log
        level is only configured after loading finishes, so a warning
        emitted during load would be filtered out. The caller invokes it
        once the level is set.
    */
    void
    reportUnknownConfigKeys() const;

private:
    /** Keys found in the configuration file that match no known option.

        Populated during load and surfaced by @ref reportUnknownConfigKeys.
    */
    std::vector<std::string> unknownConfigKeys;
};

// Config adds no reflected options of its own; it only inherits the
// schema. Describing that inheritance lets the reflection-to-DOM bridge
// project a Config directly, without a cast to its ConfigSchema base.
MRDOCS_DESCRIBE_STRUCT(Config, (ConfigSchema), ())

//------------------------------------------------

/** Parse a YAML document into a DOM object.

    Unknown keys are preserved and scalars are converted to the matching DOM
    type (integer, then boolean, then null, otherwise string). The result is
    empty when the YAML is empty or its root is not a mapping.

    @param yaml The YAML document to parse.
    @return The parsed DOM object.
*/
dom::Object
toDomObject(std::string_view yaml);

} // mrdocs


#endif // MRDOCS_API_CONFIG_HPP
