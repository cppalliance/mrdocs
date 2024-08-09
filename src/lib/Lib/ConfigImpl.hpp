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
    struct access_token {};

    struct SettingsImpl : Settings
    {
        /** Symbol filter root node.

            Root node of a preparsed tree of FilterNodes
            used during AST traversal to determine whether
            a symbol should be extracted.
        */
        FilterNode symbolFilter;

        /** Namespaces for symbols rendered as "see-below".
         */
        std::vector<FilterPattern> seeBelowFilter;

        /** Namespaces for symbols rendered as "implementation-defined".
         */
        std::vector<FilterPattern> implementationDefinedFilter;
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
    ThreadPool& threadPool_;
    SettingsImpl settings_;
    dom::Object configObj_;

    friend class Config;
    friend class Options;

    template<class T>
    friend struct llvm::yaml::MappingTraits;

public:
    ConfigImpl(access_token, ThreadPool& threadPool);

    /** Create a configuration by loading a YAML file.

        This function attempts to load the given
        YAML file and apply the results to create
        a configuration. The working directory of
        the config object will be set to the
        directory containing the file.

        If the `publicSettings` object is not empty, then
        after the YAML file is applied the settings
        will be parsed and the results will
        be applied to the configuration. Any keys
        and values in the `publicSettings` which are the
        same as elements from the file will replace
        existing settings.

        @return The configuration upon success,
        otherwise an error.

        @param publicSettings A Config::Settings object
        containing additional settings which will be parsed and
        applied to the existing configuration.

        @param threadPool The thread pool to use.
    */
    static
    Expected<std::shared_ptr<ConfigImpl const>>
    load(
        Config::Settings const& publicSettings,
        Config::Settings::ReferenceDirectories const& dirs,
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
    shouldVisitSymbol(
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
};

//------------------------------------------------


} // mrdocs
} // clang

#endif
