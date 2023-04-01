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

#ifndef MRDOX_CLANGDOCCONTEXT_HPP
#define MRDOX_CLANGDOCCONTEXT_HPP

#include "Generators.h"
#include "Representation.h"
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/Execution.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringRef.h>
#include <memory>
#include <string>

namespace clang {
namespace mrdox {

/** State information for a complete run of the tool
*/
struct ClangDocContext
{
    ClangDocContext(
        ClangDocContext&&) = delete;
    ClangDocContext& operator=(
        ClangDocContext&&) = delete;

    ClangDocContext();

    std::unique_ptr<tooling::ToolExecutor> Executor;

    tooling::ArgumentsAdjuster ArgAdjuster;

    tooling::ExecutionContext* ECtx;

    // Name of project being documented.
    std::string ProjectName;

    // Indicates if only public declarations are documented.
    bool PublicOnly;

    // Directory for outputting generated files.
    std::string OutDirectory;

    // Directory where processed files are stored. Links
    // to definition locations will only be generated if
    // the file is in this dir.
    std::string SourceRoot;     
                                                      
    // URL of repository that hosts code used
    // for links to definition locations.
    std::optional<std::string> RepositoryUrl;

    bool IgnoreMappingFailures = false;

    std::unique_ptr<Generator> G;

    Index Idx;
};

/** Set up a docs context from command line arguments.
*/
llvm::Error
setupContext(
    ClangDocContext& CDCtx,
    int argc, const char** argv);

/** Set up a docs context from command line arguments.
*/
llvm::Error
setupContext(
    ClangDocContext& CDCtx,
    llvm::SmallVector<llvm::StringRef, 16> const& args);

} // namespace mrdox
} // namespace clang

#endif
