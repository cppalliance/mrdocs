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
*/
class Config
{
    template<class T>
    friend struct llvm::yaml::MappingTraits;

    std::string sourceRoot_;

public:
    /** The root path from which all relative paths are calculated.
    */
    llvm::SmallString<16> configPath;

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
    Config() = default;
    Config(Config&&) = delete;
    Config& operator=(Config&&) = delete;

    //--------------------------------------------
    //
    // Observers
    //
    //--------------------------------------------

    llvm::StringRef
    sourceRoot() const noexcept
    {
        return sourceRoot_;
    }

    /** Returns true if the file should be skipped.

        If the file is not skipped, then prefixPath
        is set to the portion of the file path which
        should be be removed for matching files.
    */
    bool
    filterSourceFile(
        llvm::StringRef filePath,
        llvm::SmallVectorImpl<char>& prefixPath) const noexcept;

    //--------------------------------------------
    //
    // Modifiers
    //
    //--------------------------------------------

    /** Set the directory where the input files are stored.

        Symbol documentation will not be emitted unless
        the corresponding source file is a child of this
        directory.
    */
    llvm::Error
    setSourceRoot(
        llvm::StringRef dirPath);

public:
    struct filter { std::vector<std::string> include, exclude; };

    filter namespaces, files, entities;

    bool
    loadFromFile(
        llvm::StringRef filePath,
        Reporter& R);
};

} // mrdox
} // clang

#endif
