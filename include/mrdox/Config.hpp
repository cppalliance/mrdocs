//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_CONFIG_HPP
#define MRDOX_API_CONFIG_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Support/Error.hpp>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace clang {
namespace mrdox {

class ThreadPool;

/** Configuration used to generate the Corpus and Docs

    This contains all the settings applied from
    the command line and the YML file (if any).
    A configuration is always connected to a
    particular directory from which absolute paths
    are calculated from relative paths.
*/
class MRDOX_VISIBLE
    Config
{
protected:
    Config() noexcept;

public:
    /** Destructor.
    */
    MRDOX_DECL
    virtual
    ~Config() noexcept = 0;

    /** Return a pool of threads for executing work.
    */
    MRDOX_DECL
    virtual
    ThreadPool&
    threadPool() const noexcept = 0;

    //--------------------------------------------
    //
    // YAML
    //
    //--------------------------------------------

    /** `true` if AST visitation failures should not stop the program.

        @code
        ignore-failures: true
        @endcode
    */
    bool ignoreFailures = false;

    /** `true` if output should consist of multiple files.
    */
    bool multiPage = false;

    /** `true` if tool output should be verbose.

        @code
        verbose: true
        @endcode
    */
    bool verboseOutput = false;

    /** `true` if private members should be extracted and displayed.

        In some cases private members will be listed
        even if this configuration value is set to
        `false`. For example, when extracting private
        virtual functions in a base class.
    */
    bool includePrivate = false;

    /** `true` if anonymous namespace members should be extracted and displayed.

        In some cases anonymous namespace members will
        be listed even if this configuration value is set to
        `false`. For example, this may occur for a class derived
        from one declared within an anonymous namespace.
    */
    bool includeAnonymous = true;

    /** The level of concurrency desired.

        This will always be greater than zero.
    */
    unsigned int concurrency = 0;

    //--------------------------------------------

    /** Full path to the working directory

        The working directory is used to calculate
        full paths from relative paths.

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

    /** Regular expression to exclude namespaces */
    std::vector<std::string> excludeNamespaces;
};

} // mrdox
} // clang

#endif
