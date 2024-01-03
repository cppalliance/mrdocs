//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_TOOL_MR_DOCS_COMPILATION_DATABASE_HPP
#define MRDOCS_LIB_TOOL_MR_DOCS_COMPILATION_DATABASE_HPP

#include <mrdocs/Config.hpp>
#include <clang/Tooling/JSONCompilationDatabase.h>
#include <llvm/ADT/StringMap.h>
#include <vector>

namespace clang {
namespace mrdocs {

/**
 * Execute cmake to export compile_commands.json.
*/
std::optional<std::string> 
executeCmakeExportCompileCommands(llvm::StringRef cmakePath, llvm::StringRef cmakeListsPath);

/** A compilation database where all paths are absolute.

    All relative paths in the compilation database
    will be converted to absolute paths by resolving
    them according to the working directory specified
    at construction.
*/
class MrDocsCompilationDatabase
    : public tooling::CompilationDatabase
{
    std::vector<tooling::CompileCommand> AllCommands_;
    llvm::StringMap<std::size_t> IndexByFile_;

public:
    /**
     * Constructor.
     *
     * This copies the contents of the source compilation 
     * database. Every relative path is converted into an 
     * absolute path by resolving against the specified
     * working directory.
     *
     * @param workingDir The working directory against which relative paths will be resolved.
     * @param inner The source compilation database to copy.
     * @param config The shared configuration object.
     * @param implicitIncludeDirectories A map from compiler executable paths to their respective
     *        implicit include directories, as determined by the system's compiler.
     */
    MrDocsCompilationDatabase(
        llvm::StringRef workingDir,
        CompilationDatabase const& inner,
        std::shared_ptr<const Config> config,
        std::unordered_map<std::string, std::vector<std::string>> const& implicitIncludeDirectories);

    std::vector<tooling::CompileCommand>
    getCompileCommands(
        llvm::StringRef FilePath) const override;

    std::vector<std::string>
    getAllFiles() const override;

    std::vector<tooling::CompileCommand>
    getAllCompileCommands() const override;
};

} // mrdocs
} // clang

#endif // MRDOCS_LIB_TOOL_MR_DOCS_COMPILATION_DATABASE_HPP

