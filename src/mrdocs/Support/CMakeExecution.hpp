//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Fernando Pelliccioni (fpelliccioni@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_CMAKEEXECUTION_HPP
#define MRDOCS_LIB_SUPPORT_CMAKEEXECUTION_HPP

#include <mrdocs/Support/Error/Expected.hpp>
#include <llvm/ADT/StringRef.h>
#include <string>


namespace mrdocs {

/**
 * Executes CMake to generate the `compile_commands.json` file for a project.
 *
 * This function runs CMake in a temporary directory for the given project path
 * to create a `compile_commands.json` file.
 *
 * @param projectPath The path to the project directory.
 * @param cmakeArgs The arguments to pass to CMake when generating the compilation database.
 * @param tempDir The path to the temporary directory to use for CMake execution.
 * @param workingDir If non-empty, MrDocs `chdir`s to this directory for the
 *                   duration of the CMake invocation. CMake resolves relative
 *                   paths in cache assignments (e.g. `-D CMAKE_PREFIX_PATH=...`)
 *                   against its current working directory; passing the
 *                   `mrdocs.yml` directory here makes those relative paths
 *                   behave like the other path-bearing fields in the config.
 *                   The previous working directory is restored on return.
 * @return An `Expected` object containing the path to the generated `compile_commands.json` file if successful.
 *         Returns `Unexpected` if the project path is not found or if CMake execution fails.
*/
Expected<std::string>
executeCmakeExportCompileCommands(
    llvm::StringRef projectPath,
    llvm::StringRef cmakeArgs,
    llvm::StringRef tempDir,
    llvm::StringRef workingDir = llvm::StringRef());

} // mrdocs


#endif // MRDOCS_LIB_SUPPORT_CMAKEEXECUTION_HPP

