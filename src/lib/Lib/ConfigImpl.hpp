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

#ifndef MRDOCS_LIB_CONFIGIMPL_HPP
#define MRDOCS_LIB_CONFIGIMPL_HPP

#include "lib/Lib/Filters.hpp"
#include "lib/Support/YamlFwd.hpp"
#include <mrdocs/Config.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/ThreadPool.hpp>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/ThreadPool.h>
#include <memory>

namespace clang {
namespace mrdocs {

/* Private configuration implementation.

   This class is used internally to hold the
   configuration settings. It is not part of
   the public API and plugins should not use
   it.

 */
class ConfigImpl
    : public Config
    , public std::enable_shared_from_this<ConfigImpl>
{
public:
    struct SettingsImpl : Settings
    {
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

        ExtractPolicy inaccessibleBases = ExtractPolicy::Always;

        /** Additional defines passed to the compiler.
        */
        std::vector<std::string> defines;

        /** `true` if AST visitation failures should not stop the program.

            @code
            ignore-failures: true
            @endcode
        */
        bool ignoreFailures = false;

        /** The full path to the source root directory.

            The returned path will always be POSIX
            style and have a trailing separator.
        */
        std::string sourceRoot;

        /** Specifies files that should be filtered
        */
        struct FileFilter
        {
            /// Directories to include
            std::vector<std::string> include;
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

        /** Symbol filter root node.

            Root node of a preparsed tree of FilterNodes
            used during AST traversal to determine whether
            a symbol should be extracted.
        */
        FilterNode symbolFilter;

        /** The base URL for the generated documentation.

            This is used to generate links to sources
            files referenced in the documentation.
         */
        std::string baseURL;
    };

    /// @copydoc Config::settings()
    Settings const&
    settings() const noexcept override
    {
        return settings_;
    }

    /// @copydoc Config::settings()
    constexpr SettingsImpl const*
    operator->() const noexcept
    {
        return &settings_;
    }

    /// @copydoc Config::object()
    dom::Object const&
    object() const override;

private:
    SettingsImpl settings_;
    ThreadPool& threadPool_;
    llvm::SmallString<0> outputPath_;
    std::vector<std::string> inputFileIncludes_;
    dom::Object configObj_;

    friend class Config;
    friend class Options;

    template<class T>
    friend struct llvm::yaml::MappingTraits;

public:
    ConfigImpl(
        llvm::StringRef workingDir,
        llvm::StringRef addonsDir,
        llvm::StringRef configYaml,
        llvm::StringRef extraYaml,
        std::shared_ptr<ConfigImpl const> baseConfig,
        ThreadPool& threadPool);

    ThreadPool&
    threadPool() const noexcept override
    {
        return threadPool_;
    }

    //--------------------------------------------
    //
    // Private Interface
    //
    //--------------------------------------------

    /** Returns true if the translation unit should be visited.

        @param filePath The posix-style full path
        to the file being processed.
    */
    bool
    shouldVisitTU(
        llvm::StringRef filePath) const noexcept;

    /** Returns true if the file should be visited.

        If the file is visited, then prefix is
        set to the portion of the file path which
        should be be removed for matching files.

        @param filePath A posix-style full or
        relative path to the file being processed.
        Relative paths are resolved against the
        working directory.

        @param prefix The prefix which should be
        removed from subsequent matches.
    */
    bool
    shouldExtractFromFile(
        llvm::StringRef filePath,
        std::string& prefix) const noexcept;

    friend
    Expected<std::shared_ptr<ConfigImpl const>>
    createConfig(
        std::string_view workingDir,
        std::string_view addonsDir,
        std::string_view configYaml,
        ThreadPool& threadPool);

    friend
    Expected<std::shared_ptr<ConfigImpl const>>
    loadConfigFile(
        std::string_view filePath,
        std::string_view addonsDir,
        std::string_view extraYaml,
        std::shared_ptr<ConfigImpl const>,
        ThreadPool& threadPool);
};

//------------------------------------------------

/** Create a configuration by loading a YAML file.

    This function attemtps to load the given
    YAML file and apply the results to create
    a configuration. The working directory of
    the config object will be set to the
    directory containing the file.

    If the `extraYaml` string is not empty, then
    after the YAML file is applied the string
    will be parsed as YAML and the results will
    be applied to the configuration. And keys
    and values in the extra YAML which are the
    same as elements from the file will replace
    existing settings.

    @return The configuration upon success,
    otherwise an error.

    @param workingDir The path to consider as the
    working directory when resolving relative paths
    in the configuration and compilation databases.
    Relative paths will be resolved according to
    the current working directory of the process.
    POSIX or Windows style path separators are
    accepted.

    @param addonsDir An optional, valid directory
    to determine the location of the addons/ folder.
    If the path is not absolute, it will be resolved
    relative to the current working directory of
    the process.

    @param configYaml A string containing YAML
    which will be parsed to produce the
    configuraton settings.

    @param threadPool The thread pool to use.
*/
Expected<std::shared_ptr<ConfigImpl const>>
createConfig(
    std::string_view workingDir,
    std::string_view addonsDir,
    std::string_view configYaml,
    ThreadPool& threadPool);

/** Create a configuration by loading a YAML file.

    This function attemtps to load the given
    YAML file and apply the results to create
    a configuration. The working directory of
    the config object will be set to the
    directory containing the file.

    If the `extraYaml` string is not empty, then
    after the YAML file is applied the string
    will be parsed as YAML and the results will
    be applied to the configuration. And keys
    and values in the extra YAML which are the
    same as elements from the file will replace
    existing settings.

    @return The configuration upon success,
    otherwise an error.

    @param filePath A relative or absolute path
    to the configuration file. Relative paths will
    be resolved according to the current working
    directory of the process. POSIX or Windows
    style path separators are accepted.

    @param addonsDir An optional, valid directory
    to determine the location of the addons/ folder.
    If the path is not absolute, it will be resolved
    relative to the current working directory of
    the process.

    @param extraYaml An optional string containing
    additional valid YAML which will be parsed and
    applied to the existing configuration.

    @param baseConfig A configuration to copy
    first, or nullptr if unspecified. This has
    the effect of inheriting settings.

    @param threadPool The thread pool to use.
*/
MRDOCS_DECL
Expected<std::shared_ptr<ConfigImpl const>>
loadConfigFile(
    std::string_view filePath,
    std::string_view addonsDir,
    std::string_view extraYaml,
    std::shared_ptr<ConfigImpl const> baseConfig,
    ThreadPool& threadPool);

} // mrdocs
} // clang

#endif
