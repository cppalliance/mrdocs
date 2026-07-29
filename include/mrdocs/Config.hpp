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
#include <string>
#include <string_view>

namespace llvm::yaml {

template<class T>
struct MappingTraits;

} // llvm::yaml


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
    /** Load the configuration options from a YAML string.

        Populates the options from the YAML and records the DOM view. Paths
        are resolved later by @ref normalize.

        @param c The configuration to populate.
        @param configYaml The configuration YAML.
        @return Nothing on success, otherwise an error.
    */
    static
    Expected<void>
    load(
        Config& c,
        std::string_view configYaml);

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

    /** Return a DOM object representing the configuration keys.

        The object is valid for the lifetime of the configuration.

        @return The configuration as a DOM object.
    */
    dom::Object const&
    object() const
    {
        return configObj_;
    }

private:
    dom::Object configObj_;

    template<class T>
    friend struct llvm::yaml::MappingTraits;

    /** Overlay the typed option values onto the DOM view.
    */
    void
    updateConfigDom();
};

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
