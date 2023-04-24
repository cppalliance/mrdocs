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
#include <string>

namespace llvm {
namespace yaml {
template<class T>
struct MappingTraits;
} // yaml
} // llvm

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
    Config(Config const&) = delete;
    Config& operator=(Config const&) = delete;

protected:
    llvm::SmallString<0> configDir_;
    llvm::SmallString<0> outputPath_;
    std::string sourceRoot_;
    bool includePrivate_ = false;
    bool verbose_ = true;

    explicit
    Config(llvm::StringRef configDir);

public:
    class Options; // private, but for clang-15 bug

    /** A resource for running submitted work, possibly concurrent.
    */
    class WorkGroup;

    /** Destructor.
    */
    MRDOX_DECL
    virtual
    ~Config() noexcept;

    //
    // VFALCO these naked data members are temporary...
    //
    tooling::ArgumentsAdjuster ArgAdjuster;
    bool IgnoreMappingFailures = false;

    //--------------------------------------------
    //
    // Observers
    //
    //--------------------------------------------

    /** Return true if tools should show progress.
    */
    bool
    verbose() const noexcept
    {
        return verbose_;
    }

    /** Return the full path to the configuration directory.

        The returned path will always be POSIX
        style and have a trailing separator.
    */
    llvm::StringRef
    configDir() const noexcept
    {
        return configDir_;
    }

    /** Return the full path to the source root directory.

        The returned path will always be POSIX
        style and have a trailing separator.
    */
    llvm::StringRef
    sourceRoot() const noexcept
    {
        return sourceRoot_;
    }

    /** Return the output directory or filename.
    */
    llvm::StringRef
    outputPath() const noexcept
    {
        return outputPath_;
    }

    /** Return true if private members are documented.
    */
    bool
    includePrivate() const noexcept
    {
        return includePrivate_;
    }

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
    // Modifiers
    //
    //--------------------------------------------

    /** Set whether tools should show progress.
    */
    void
    setVerbose(
        bool verbose) noexcept
    {
        verbose_ = verbose;
    }

    /** Set whether or not to include private members.
    */
    void
    setIncludePrivate(
        bool includePrivate) noexcept
    {
        includePrivate_ = includePrivate;
    }

    /** Set the output directory or file path.
    */
    MRDOX_DECL
    virtual
    void
    setOutputPath(
        llvm::StringRef outputPath) = 0;

    /** Set the directory where the input files are stored.

        Symbol documentation will not be emitted unless
        the corresponding source file is a child of this
        directory.

        If the specified directory is relative, then
        the full path will be computed relative to
        @ref configDir().

        @param dirPath The directory.
    */
    MRDOX_DECL
    virtual
    void
    setSourceRoot(
        llvm::StringRef dirPath) = 0;

    /** Set the filter for including translation units.
    */
    MRDOX_DECL
    virtual
    void
    setInputFileIncludes(
        std::vector<std::string> const& list) = 0;

    //--------------------------------------------
    //
    // Creation
    //
    //--------------------------------------------

    /** Return a defaulted Config using an existing directory.

        @param dirPath The path to the directory.
        If this is relative, an absolute path will
        be calculated from the current directory.
    */
    MRDOX_DECL
    static
    llvm::Expected<std::shared_ptr<Config>>
    createAtDirectory(
        llvm::StringRef dirPath);

    /** Return a Config loaded from the specified YAML file.
    */
    MRDOX_DECL
    static
    llvm::Expected<std::shared_ptr<Config>>
    loadFromFile(
        llvm::StringRef filePath);
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
    ~WorkGroup() noexcept;

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

//------------------------------------------------

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
