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
    struct Settings
    {
        //--------------------------------------------
        // Command line options

        /** Full path to the config file directory

            The working directory is the directory
            of the mrdocs.yml file.

            It is used to calculate full paths
            from relative paths.

            This string will always be native style
            and have a trailing directory separator.
        */
        std::string configDir;

        /** A string holding the complete configuration YAML.
         */
        std::string config = "./mrdocs.yml";

        /** A string holding the complete configuration YAML.
         */
        std::string configYaml;


        // -------------------------------------------
        // Public config options
        /// Path to the Addons directory
        std::string addons;

        /// Path to the source root directory
        std::string sourceRoot;

        /// Directory or file for generating output
        std::string output;

        /// Path to the compilation database or build script
        std::string compilationDatabase;

        /// CMake arguments when generating the compilation database
        /// from CMakeLists.txt
        std::string cmake;

        /// LibC++ include paths
        std::vector<std::string> libCxxPaths;

        /// Additional defines passed to the compiler
        std::vector<std::string> defines;

        /// Documentation generator. Supported generators are: adoc, html, xml
        std::string generate = "adoc";

        /// True if output should consist of multiple files)
        bool multipage = false;

        /** The base URL for the generated documentation.

            This is used to generate links to source
            files referenced in the documentation.
          */
        std::string baseURL;

        /// True if more information should be output to the console)
        bool verbose = false;

        /// The minimum reporting level: 0 to 4
        unsigned report = 1;

        /// Continue if files are not mapped correctly)
        bool ignoreMapErrors = true;

        /// Whether AST visitation failures should not stop the program)
        bool ignoreFailures = false;

        /** Extraction policy for declarations.

            This determines how declarations are extracted.

         */
        enum class ExtractPolicy
        {
            /// Always extract the declaration.
            Always,
            /// Extract the declaration if it is referenced.
            Dependency,
            /// Never extract the declaration.
            Never
        };

        /** Extraction policy for references to external declarations.

            This determines how declarations which are referenced by
            explicitly extracted declarations are extracted.
         */
        ExtractPolicy referencedDeclarations = ExtractPolicy::Dependency;

        /** Extraction policy for anonymous namespace.
         */
        ExtractPolicy anonymousNamespaces = ExtractPolicy::Always;

        /** Extraction policy for inaccessible members.
         */
        ExtractPolicy inaccessibleMembers = ExtractPolicy::Always;

        /** Extraction policy for inaccessible bases.
         */
        ExtractPolicy inaccessibleBases = ExtractPolicy::Always;

        /** Specifies patterns that should be filtered
         */
        struct FileFilter
        {
            /// Directories to include
            std::vector<std::string> include;

            /// File patterns
            std::vector<std::string> filePatterns;
        };

        /// @copydoc FileFilter
        FileFilter input;

        /** Specifies filters for various kinds of symbols.
         */
        struct Filters
        {
            /** Specifies inclusion and exclusion patterns
             */
            struct Category
            {
                std::vector<std::string> include;
                std::vector<std::string> exclude;
            };

            /// Specifies filter patterns for symbols
            Category symbols;
        };

        /// @copydoc Filters
        Filters filters;

        /** Namespaces for symbols rendered as "see-below".
         */
        std::vector<std::string> seeBelow;

        /** Namespaces for symbols rendered as "implementation-defined".
         */
        std::vector<std::string> implementationDefined;

        /** Whether to detect the SFINAE idiom.
         */
        bool detectSfinae = true;

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

/** Load the public configuration settings from the specified YAML file.
 */
MRDOCS_DECL
Expected<void>
loadConfig(Config::Settings& s, std::string_view configYaml);

namespace detail
{
    template <class T>
    concept DefinesAllCmdLineOptions = requires (T const& t)
    {
        #define CMDLINE_OPTION(Name, Kebab, Desc) t.Name;
        #include <mrdocs/ConfigOptions.inc>
    };

    template <class T>
    concept DefinesAllCommonOptions = requires (T const& t)
    {
        #define COMMON_OPTION(Name, Kebab, Desc) t.Name;
        #include <mrdocs/ConfigOptions.inc>
    };

    template <class T>
    concept DefinesAllOptions =
        DefinesAllCmdLineOptions<T> &&
        DefinesAllCommonOptions<T>;
}

// This static assertion checks that required options are defined
// in the Config::Settings class. If this assertion fails, it means that
// the Config::Settings class is missing some required options.
static_assert(detail::DefinesAllCommonOptions<Config::Settings>);

} // mrdocs
} // clang

#endif
