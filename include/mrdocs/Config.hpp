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

#ifndef MRDOCS_API_CONFIG_HPP
#define MRDOCS_API_CONFIG_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Dom/Object.hpp>
#include <mrdocs/PublicSettings.hpp>
#include "lib/Support/YamlFwd.hpp"
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace clang {
namespace mrdocs {

class ThreadPool;

/** Configuration used to generate the Corpus and Docs

    This contains all the public settings applied
    from the command line and the YML file (if any).

    This class stores the original and public config
    options as they are passed to MrDocs, in their original
    data types, such as strings and bools.

    This class is an abstract interface whose private
    concrete implementation typically includes these
    parsed configuration values in a more convenient form
    for use by MrDocs.

    Meanwhile, this class is provided publicly to plugins.

    The configuration is always connected to the
    directory of the mrdocs.yml file from which
    absolute paths are calculated from relative paths.

*/
class MRDOCS_DECL
    Config
{
protected:
    Config() noexcept;

public:
    /** Settings values used to generate the Corpus and Docs
     */
    struct Settings : public PublicSettings
    {
        /**
         * @brief Loads the public configuration settings from the specified YAML file.
         *
         * This function takes a YAML file and a set of reference directories as input.
         * It parses the YAML file and loads the configuration settings into a Config::Settings object.
         * The reference directories are used to resolve any relative paths in the configuration settings.
         *
         * @param configYaml A string view representing the YAML file containing the configuration settings.
         * @param dirs A constant reference to a PublicSettings::ReferenceDirectories object containing the reference directories.
         * @return An Expected object containing a Config::Settings object if the YAML file was successfully parsed and the configuration settings were loaded, or an error otherwise.
         */
        static
        Expected<void>
        load(
            Config::Settings &s,
            std::string_view configYaml,
            ReferenceDirectories const& dirs);

        /**
         * @brief Loads the public configuration settings from the specified file.
         *
         * This function takes a file path and a set of reference directories as input.
         * It reads the file and loads the configuration settings into a Config::Settings object.
         * The reference directories are used to resolve any relative paths in the configuration settings.
         *
         * @param s A reference to a Config::Settings object where the configuration settings will be loaded.
         * @param filePath A string view representing the file path of the configuration settings.
         * @param dirs A constant reference to a PublicSettings::ReferenceDirectories object containing the reference directories.
         * @return An Expected object containing void if the file was successfully read and the configuration settings were loaded, or an error otherwise.
         */
        static Expected<void>
        load_file(
            Config::Settings &s,
            std::string_view configPath,
            ReferenceDirectories const& dirs);

        /** @copydoc PublicSettings::normalize
         */
        Expected<void>
        normalize(ReferenceDirectories const& dirs);

        //--------------------------------------------
        // Preprocessed options
        //
        // Options derived from the PublicSettings that
        // are reused often.

        /** Full path to the config file directory

            The reference directory for most MrDocs
            options is the directory of the
            mrdocs.yml file.

            It is used to calculate full paths
            from relative paths.

            This string will always be native style
            and have a trailing directory separator.
        */
        std::string
        configDir() const;

        /** Full path to the output directory

            The reference directory for MrDocs
            output and temporary files is the
            output directory.

            This is either the output option
            (if already a directory) or
            the parent directory of the output
            option (if it is a file).

            When the output option is a path
            that does not exist, we determine
            if it's a file or directory by
            checking if the filename contains
            a period.

            This string will always be native style
            and have a trailing directory separator.
        */
        std::string
        outputDir() const;

        /** Full path to the mrdocs root directory

            This is the directory containing the
            mrdocs executable and the shared files.

            This string will always be native style
            and have a trailing directory separator.
        */
        std::string mrdocsRootDir;

        /** Full path to the current working directory

            This string will always be native style
            and have a trailing directory separator.
        */
        std::string cwdDir = ".";

        /** A string holding the complete configuration YAML.
         */
        std::string configYaml;

        constexpr Settings const*
        operator->() const noexcept
        {
            return this;
        }

    private:
        template<class T>
        friend struct llvm::yaml::MappingTraits;
    };

    /** Destructor.
    */
    MRDOCS_DECL
    virtual
    ~Config() noexcept = 0;

    /** Return a pool of threads for executing work.
    */
    MRDOCS_DECL
    virtual
    ThreadPool&
    threadPool() const noexcept = 0;

    /** Return the settings used to generate the Corpus and Docs.
     */
    virtual Settings const& settings() const noexcept = 0;

    /** Return a DOM object representing the configuration keys.

        The object is invalidated when the configuration
        is moved or destroyed.

     */
    virtual dom::Object const& object() const = 0;

    /// @copydoc settings()
    constexpr Settings const*
    operator->() const noexcept
    {
        return &settings();
    }
};

} // mrdocs
} // clang

#endif
