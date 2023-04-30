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

#ifndef MRDOX_CONFIG_HPP
#define MRDOX_CONFIG_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Reporter.hpp>
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Error.h>
#include <functional>
#include <memory>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace clang {
namespace mrdox {

class ConfigImpl;

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
    /** A resource for running submitted work, possibly concurrent.
    */
    class WorkGroup;

    /** Destructor.
    */
    MRDOX_DECL
    virtual
    ~Config() noexcept = 0;

    /** Return true if tool output should be verbose.
    */
    MRDOX_DECL virtual bool
    verbose() const noexcept = 0;

    //
    // VFALCO these naked data members are temporary...
    //
    tooling::ArgumentsAdjuster ArgAdjuster;
    bool IgnoreMappingFailures = false;

    /** Call a function for each element of a range.

        The function is invoked with a reference
        to each element of the container using the
        concurrency specified in the configuration.

        This function must not be called concurrently,
        despite being marked `const`.
    */
    template<class Range, class UnaryFunction>
    void
    parallelForEach(
        Range&& range,
        UnaryFunction const& f) const;

    //--------------------------------------------
    //
    // Observers
    //
    //--------------------------------------------

    MRDOX_DECL virtual std::string_view outputPath() const noexcept = 0;
    MRDOX_DECL virtual std::string_view sourceRoot() const noexcept = 0;
    MRDOX_DECL virtual bool singlePage() const noexcept = 0;

    /** Return the YAML strings which produced this config.

        The first element of the pair is the entire
        contents of the YAML file or string used to
        apply the initial values to the configuration.

        The second element of the pair is either empty,
        or holds a complate additional YAML string which
        was applied to the values of the configuration,
        after the first string was applied.

        Any keys in the second YAML which match keys
        found in the first YAML will effectively replace
        those entries in the configuration.

        A @ref Generator that wishes to implement
        format-specific options, should parse and
        apply the first string, then parse and apply
        the second string to the same object.

        @return The pair of valid YAML strings.
    */
    MRDOX_DECL virtual auto
    configYaml() const noexcept ->
        std::pair<std::string_view, std::string_view> = 0;
};

//------------------------------------------------

/** A group representing possibly concurrent related tasks.
*/
class MRDOX_VISIBLE
    Config::WorkGroup
{
public:
    /** Destructor.
    */
    MRDOX_DECL
    ~WorkGroup();

    /** Constructor.

        Default constructed workgroups have no
        concurrency level. Calls to post and wait
        are blocking.
    */
    MRDOX_DECL
    WorkGroup() noexcept;

    /** Constructor.
    */
    MRDOX_DECL
    explicit
    WorkGroup(
        Config const* config);

    /** Constructor.
    */
    MRDOX_DECL
    WorkGroup(
        WorkGroup const& other);

    /** Assignment.
    */
    MRDOX_DECL
    WorkGroup&
    operator=(
        WorkGroup const& other);

    /** Post work to the work group.
    */
    MRDOX_DECL
    void
    post(std::function<void(void)>);

    /** Wait for all posted work in the work group to complete.
    */
    MRDOX_DECL
    void
    wait();

private:
    friend class ConfigImpl;

    struct Base
    {
        MRDOX_DECL virtual ~Base() noexcept;
    };

    class Impl;

    std::shared_ptr<ConfigImpl const> config_;
    std::unique_ptr<Base> impl_;
};

template<class Range, class UnaryFunction>
void
Config::
parallelForEach(
    Range&& range,
    UnaryFunction const& f) const
{
    WorkGroup wg(this);
    for(auto&& element : range)
        wg.post([&f, &element]
            {
                f(element);
            });
    wg.wait();
}

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

    @return A valid object upon success.

    @param fileName A full or relative path to
    the configuration file, which may have any
    extension including no extension.

    @param extraYaml An optional string containing
    additional valid YAML which will be parsed and
    applied to the existing configuration.

    @param ec [out] Set to the error, if any occurred.
*/
MRDOX_DECL
std::shared_ptr<Config const>
loadConfigFile(
    std::string_view fileName,
    std::string_view extraYaml,
    std::error_code& ec);

/** Return a configuration by loading a YAML file.

    This function attemtps to load the given
    YAML file and apply the settings to create
    a configuration. The working directory of
    the config object will be set to the
    directory containing the file.

    @return A valid object upon success.

    @param fileName A full or relative path to
    the configuration file, which may have any
    extension including no extension.

    @param ec [out] Set to the error, if any occurred.
*/
inline
std::shared_ptr<Config const>
loadConfigFile(
    std::string_view fileName,
    std::error_code& ec)
{
    return loadConfigFile(fileName, "", ec);
}

/** Create a configuration by loading a YAML string.

    This function attempts to parse the given
    YAML string and apply the results to create
    a configuration. The working directory of
    the config object will be set to the specified
    full path. If the specified path is empty,
    then the current working directory of the
    process will be used instead.

    @return A valid object upon success.

    @param workingDir The directory which
    should be considered the working directory
    for calculating filenames from relative
    paths.

    @param configYaml A string containing valid
    YAML which will be parsed and applied to create
    the configuration.

    @param ec [out] Set to the error, if any occurred.
    */
MRDOX_DECL
std::shared_ptr<Config const>
loadConfigString(
    std::string_view workingDir,
    std::string_view configYaml,
    std::error_code& ec);

//------------------------------------------------

} // mrdox
} // clang

#endif
