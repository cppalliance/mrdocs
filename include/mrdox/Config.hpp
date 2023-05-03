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
    // YAML
    //
    //--------------------------------------------

    /** `true` if AST visitation failures should not stop the program.

        @code
        ignore-failures: true
        @endcode
    */
    bool ignoreFailures = false;

    /** `true` if everything should be output to a single page.
    */
    bool singlePage = false;

    /** `true` if tool output should be verbose.

        @code
        verbose: true
        @endcode
    */
    bool verboseOutput = false;

    //--------------------------------------------

    /** A string holding the complete configuration YAML.
    */
    std::string_view configYaml;

    /** A string holding extra configuration YAML.

        Any keys in this string which match keys used
        in @ref configYaml will effectively replace
        those entries in the configuration.

        A @ref Generator that wishes to implement
        format-specific options, should parse and
        apply `configYaml`, then parse and apply
        this string to the same settings.
    */
    std::string_view extraYaml;
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

} // mrdox
} // clang

#endif
