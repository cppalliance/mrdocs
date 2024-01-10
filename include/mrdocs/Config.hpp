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

    This contains all the settings applied from
    the command line and the YML file (if any).
    A configuration is always connected to a
    particular directory from which absolute paths
    are calculated from relative paths.

    The Config class is an abstract interface
    whose concrete implementation includes
    .
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
        /** Selected documentation generator.

            The currently supported generators are:

            @li `adoc`: Generates documentation in the AsciiDoc format.
            @li `html`: Generates documentation as plain HTML.
            @li `xml`: Generates an XML representation of the corpus.
        */
        std::string generate = "adoc";

        /** `true` if output should consist of multiple files.
        */
        bool multiPage = false;

        //--------------------------------------------

        /** Full path to the working directory

            The working directory is the directory
            of the mrdocs.yml file.

            It is used to calculate full paths
            from relative paths.

            This string will always be native style
            and have a trailing directory separator.
        */
        std::string workingDir;

        /** Full path to the Addons directory.

            This string will always be native style
            and have a trailing directory separator.
        */
        std::string addonsDir;

        /** A string holding the complete configuration YAML.
        */
        std::string configYaml;

        /** A string holding extra configuration YAML.

            Any keys in this string which match keys used
            in @ref configYaml will effectively replace
            those entries in the configuration.

            A @ref Generator that wishes to implement
            format-specific options, should parse and
            apply `configYaml`, then parse and apply
            this string to the same settings.
        */
        std::string extraYaml;

        constexpr Settings const*
        operator->() const noexcept
        {
            return this;
        }
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
