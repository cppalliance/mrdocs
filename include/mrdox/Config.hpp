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
#include <llvm/ADT/Optional.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/YAMLTraits.h>
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

/** Configuration used to generate the Corpus and Docs

    This contains all the settings applied from
    the command line and the YML file (if any).
    A configuration is always connected to a
    particular directory from which absolute paths
    are calculated from relative paths.
*/
class Config : public std::enable_shared_from_this<Config>
{
    template<class T>
    friend struct llvm::yaml::MappingTraits;

    class Impl;
    struct Options;

    llvm::SmallString<0> configDir_;
    std::string sourceRoot_;
    std::vector<llvm::SmallString<0>> inputFileIncludes_;
    bool verbose_ = true;
    bool includePrivate_ = false;

    llvm::SmallString<0>
    normalizePath(llvm::StringRef pathName);

protected:
    explicit Config(llvm::StringRef configDir);

public:
    virtual ~Config() = default;

    /** Return a defaulted Config using an existing directory.

        @param dirPath The path to the directory.
        If this is relative, an absolute path will
        be calculated from the current directory.
    */
    static
    llvm::Expected<std::shared_ptr<Config>>
    createAtDirectory(
        llvm::StringRef dirPath);

    /** Return a Config loaded from the specified YAML file.
    */
    static
    llvm::Expected<std::shared_ptr<Config>>
    loadFromFile(
        llvm::StringRef filePath);

    //
    // VFALCO these naked data members are temporary...
    //

    /** Adjustments to tool command line, applied during execute.
    */
    tooling::ArgumentsAdjuster ArgAdjuster;

    /** Name of project being documented.
    */
    std::string ProjectName;

    // Directory for outputting generated files.
    std::string OutDirectory;
                                                     
    // URL of repository that hosts code used
    // for links to definition locations.
    llvm::Optional<std::string> RepositoryUrl;

    bool IgnoreMappingFailures = false;

public:
    /** A resource for running submitted work, possibly concurrent.
    */
    class WorkGroup;

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

    /** Return true if private members are documented.
    */
    bool
    includePrivate() const noexcept
    {
        return includePrivate_;
    }

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

        @param filePath The posix-style full path
        to the file being processed.

        @param prefix The prefix which should be
        removed from subsequent matches.
    */
    bool
    shouldVisitFile(
        llvm::StringRef filePath,
        llvm::SmallVectorImpl<char>& prefix) const noexcept;

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

    /** Set the directory where the input files are stored.

        Symbol documentation will not be emitted unless
        the corresponding source file is a child of this
        directory.

        If the specified directory is relative, then
        the full path will be computed relative to
        @ref configDir().

        @param dirPath The directory.
    */
    void
    setSourceRoot(
        llvm::StringRef dirPath);

    /** Set the filter for including translation units.
    */
    void
    setInputFileIncludes(
        std::vector<std::string> const& list);
};

//------------------------------------------------

/** A group representing possibly concurrent related tasks.
*/
class Config::WorkGroup
{
public:
    /** Destructor.
    */
    ~WorkGroup();

    /** Constructor.

        Default constructed workgroups have no
        concurrency level. Calls to post and wait
        are blocking.
    */
    WorkGroup() noexcept = default;

    /** Constructor.
    */
    explicit
    WorkGroup(
        std::shared_ptr<Config const> config);

    /** Constructor.
    */
    WorkGroup(WorkGroup const& other);

    /** Assignment.
    */
    WorkGroup&
    operator=(WorkGroup const& other);

    /** Post work to the work group.
    */
    void post(std::function<void(void)>);

    /** Wait for all posted work in the work group to complete.
    */
    void wait();

private:
    friend class Config::Impl;

    struct Base
    {
        virtual ~Base() = default;
    };

    class Impl;

    std::shared_ptr<Config::Impl const> config_;
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
    WorkGroup wg(shared_from_this());
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
