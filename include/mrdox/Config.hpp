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

#include "Generators.h"
#include "Representation.h"
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/Execution.h>
#include <llvm/ADT/Optional.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <memory>
#include <source_location>
#include <string>

namespace clang {
namespace mrdox {

/** Configuration used to generate the Corpus and Docs

    This contains all the settings applied from the
    command line and the YML file (if any).
*/
struct Config
{
    Config();
    Config(Config&&) = delete;
    Config& operator=(Config&&) = delete;

    std::unique_ptr<tooling::ToolExecutor> Executor;

    tooling::ArgumentsAdjuster ArgAdjuster;

    // Name of project being documented.
    std::string ProjectName;

    // Indicates if only public declarations are documented.
    bool PublicOnly = true;

    // Directory for outputting generated files.
    std::string OutDirectory;

    // Directory where processed files are stored. Links
    // to definition locations will only be generated if
    // the file is in this dir.
    std::string SourceRoot;
                                                      
    // URL of repository that hosts code used
    // for links to definition locations.
    llvm::Optional<std::string> RepositoryUrl;

    bool IgnoreMappingFailures = false;

    struct filter { std::vector<std::string> include, exclude; };

    filter namespaces, files, entities;

    std::unique_ptr<Generator> G;

    std::error_code load(const std::string & name = ".mrdox.yaml",
                         const std::source_location & loc = std::source_location::current());
};

} // mrdox
} // clang

#endif
