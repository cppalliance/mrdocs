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
#include <mrdocs/Config/ReferenceDirectories.hpp>
#include <mrdocs/ConfigSchema.hpp>
#include <mrdocs/Dom/Object.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <mrdocs/Support/Reporter.hpp>
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
    static Expected<void>
    loadFile(
        Config& c,
        std::string_view configPath);

    /** Load a config file, apply command-line overrides, and finalize it.

        The single entry point for the common case: it reads @p configPath,
        applies the `--option` overrides parsed from @p argv on top of it,
        normalizes and validates everything against @p dirs, sets the
        level of @p reporter to the configured log level (a caller may start
        it high so option parsing stays quiet), and reports any unrecognized
        keys. @p argv is a null-terminated array; use the overload without
        it when there is no command line to apply.

        @param c The configuration to populate.
        @param configPath The path to the configuration file.
        @param dirs The reference directories used to resolve relative paths.
        @param argv The command line whose `--option` values override the file.
        @param reporter The reporter that prints the warnings about the
        options. This function sets its level to the configured `log-level`
        and its `warn-as-error` flag to the configured value, before the
        values are normalized and the unrecognized keys are reported, and
        leaves them set.
        @return Nothing on success, otherwise an error.
    */
    static Expected<void>
    loadFile(
        Config& c,
        std::string_view configPath,
        ReferenceDirectories const& dirs,
        char const** argv,
        report::Reporter& reporter);

    /// @copydoc loadFile(Config&, std::string_view, ReferenceDirectories const&, char const**, report::Reporter&)
    static Expected<void>
    loadFile(
        Config& c,
        std::string_view configPath,
        ReferenceDirectories const& dirs,
        report::Reporter& reporter);

    /** Load a configuration from a YAML string, apply command-line overrides, and finalize it.

        Does what @ref loadFile does once it has read the file: it loads
        @p configYaml with the `--option` overrides parsed from @p argv on
        top, sets the level of @p reporter to the configured log level,
        normalizes and validates everything against @p dirs, and reports any
        unrecognized or deprecated keys.

        @p configPath is the path the YAML stands for. It becomes the
        `config` option, so the directory the relative paths resolve against
        (see @ref configDir) is its directory. The file is not read, and it
        need not exist: the configuration records that it was loaded from a
        string, and @ref normalize skips the existence check of the `config`
        option for it. Every other option is checked as usual.

        @p argv is a null-terminated array; use the overload without it when
        there is no command line to apply.

        @param c The configuration to populate.
        @param configYaml The configuration YAML (may be empty).
        @param configPath The path of the file the YAML stands for.
        @param dirs The reference directories used to resolve relative paths.
        @param argv The command line whose `--option` values override the YAML.
        @param reporter The reporter that prints the warnings about the
        options. This function sets its level to the configured `log-level`
        and its `warn-as-error` flag to the configured value, before the
        values are normalized and the unrecognized keys are reported, and
        leaves them set.
        @return Nothing on success, otherwise an error.
    */
    static Expected<void>
    loadString(
        Config& c,
        std::string_view configYaml,
        std::string_view configPath,
        ReferenceDirectories const& dirs,
        char const** argv,
        report::Reporter& reporter);

    /// @copydoc loadString(Config&, std::string_view, std::string_view, ReferenceDirectories const&, char const**, report::Reporter&)
    static Expected<void>
    loadString(
        Config& c,
        std::string_view configYaml,
        std::string_view configPath,
        ReferenceDirectories const& dirs,
        report::Reporter& reporter);

    /** Normalize the options against the reference directories.

        Applies defaults, validates values, and resolves relative paths.
        Call this after any command-line overrides are applied.

        @param dirs The reference directories used to resolve relative paths.
        @param reporter The reporter that prints the warnings about the
        values, such as a path that does not exist.
        @return Nothing on success, otherwise an error.
    */
    Expected<void>
    normalize(
        ReferenceDirectories const& dirs,
        report::Reporter& reporter);

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

        @param reporter The reporter to report the keys to.
    */
    void
    reportUnknownConfigKeys(report::Reporter& reporter) const;

    /** Warn about deprecated options that were supplied.

        Each warning carries the deprecation note the schema records for
        the option. Deferred for the same reason as
        @ref reportUnknownConfigKeys: the log level is configured only
        after loading finishes.

        @param reporter The reporter to report the options to.
    */
    void
    reportDeprecatedConfigKeys(report::Reporter& reporter) const;

    /** Handle a supplied option that the schema marks deprecated.

        Records it for @ref reportDeprecatedConfigKeys and forwards its
        value onto the option that replaced it, when the schema names one.

        @pre `ConfigSchema::isDeprecated(key)`

        @param key The option name as supplied in the file or on the
        command line.
    */
    void
    handleDeprecatedKey(std::string_view key);

private:
    /** Whether the configuration was loaded from a string.

        Set by @ref loadString and never by @ref loadFile. @ref normalize
        skips only the existence check of the `config` option when it is set.
    */
    bool loadedFromString = false;

    /** Set the `config` option, load the YAML, and finalize the configuration.

        The steps @ref loadFile and @ref loadString share.

        @param fromString The value for @ref loadedFromString.
    */
    static Expected<void>
    loadAndNormalize(
        Config& c,
        std::string_view configYaml,
        std::string_view configPath,
        ReferenceDirectories const& dirs,
        char const** argv,
        report::Reporter& reporter,
        bool fromString);

    /** Keys found in the configuration file that match no known option.

        Populated during load and surfaced by @ref reportUnknownConfigKeys.
    */
    std::vector<std::string> unknownConfigKeys;

    /** Deprecated options supplied in the file or on the command line.

        Populated during load and surfaced by
        @ref reportDeprecatedConfigKeys.
    */
    std::vector<std::string> deprecatedConfigKeys;
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
MRDOCS_DECL
dom::Object
toDomObject(std::string_view yaml);

} // mrdocs


#endif // MRDOCS_API_CONFIG_HPP
