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

/** A compilation database for MrDocs.

    This class is a type of `clang::tooling::CompilationDatabase`
    where each compile command is adjusted to match the
    requirements of MrDocs.

    - Relative paths are converted to absolute paths,
    - Paths are converted to native format,
    - Implicit include directories are added to the compile commands,
    - Custom configuration macros are added to the compile commands,
    - Non C++ files are filtered out.
    - Warnings are disabled

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
     * database. Each compile command is adjusted to match
     * the requirements of MrDocs.
     *
     * @param workingDir The working directory against which relative paths will be resolved.
     * @param inner The source compilation database to copy.
     * @param config The shared configuration object.
     * @param LibC++ include directories.
     */
    MrDocsCompilationDatabase(
        llvm::StringRef workingDir,
        CompilationDatabase const& inner,
        std::shared_ptr<const Config> config);

    /** Get all compile commands for a file.

        @return A vector of compile commands.
    */
    std::vector<tooling::CompileCommand>
    getCompileCommands(
        llvm::StringRef FilePath) const override;

    /** Get all files in the database.

        @return A vector of file paths.
    */
    std::vector<std::string>
    getAllFiles() const override;

    /** Get all compile commands in the database.

        @return A vector of compile commands.
    */
    std::vector<tooling::CompileCommand>
    getAllCompileCommands() const override;
};

} // mrdocs
} // clang

#endif // MRDOCS_LIB_TOOL_MR_DOCS_COMPILATION_DATABASE_HPP

