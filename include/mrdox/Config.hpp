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

#include <mrdox/Reporter.hpp>
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <llvm/ADT/Optional.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/YAMLTraits.h>
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
class Config
{
    template<class T>
    friend struct llvm::yaml::MappingTraits;

    struct Options;

    llvm::SmallString<0> configDir_;
    std::string sourceRoot_;
    std::vector<std::string> inputFileFilter_;
    bool verbose_ = true;

    explicit Config(llvm::StringRef configDir);

public:
    /** Return a defaulted Config using an existing directory.

        @param dirPath The path to the directory.
        If this is relative, an absolute path will
        be calculated from the current directory.
    */
    static
    llvm::Expected<std::unique_ptr<Config>>
    createAtDirectory(
        llvm::StringRef dirPath);

    /** Return a Config loaded from the specified YAML file.
    */
    static
    llvm::Expected<std::unique_ptr<Config>>
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

    // Indicates if only public declarations are documented.
    bool PublicOnly = true;

    // Directory for outputting generated files.
    std::string OutDirectory;
                                                     
    // URL of repository that hosts code used
    // for links to definition locations.
    llvm::Optional<std::string> RepositoryUrl;

    bool IgnoreMappingFailures = false;

public:
    Config(Config&&) = delete;
    Config& operator=(Config&&) = delete;

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

    /** Set the directory where the input files are stored.

        Symbol documentation will not be emitted unless
        the corresponding source file is a child of this
        directory.
    */
    llvm::Error
    setSourceRoot(
        llvm::StringRef dirPath);

    /** Set the filter for including translation units.
    */
    llvm::Error
    setInputFileFilter(
        std::vector<std::string> const& list);
};

} // mrdox
} // clang

#endif
