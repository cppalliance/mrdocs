//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Fernando Pelliccioni (fpelliccioni@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_TOOL_CMAKE_EXECUTION_HPP
#define MRDOCS_LIB_TOOL_CMAKE_EXECUTION_HPP

#include <string>

#include <llvm/ADT/StringRef.h>
#include <mrdocs/Support/Expected.hpp>

namespace clang {
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
 * @return An `Expected` object containing the path to the generated `compile_commands.json` file if successful.
 *         Returns `Unexpected` if the project path is not found or if CMake execution fails.
 */
Expected<std::string>
executeCmakeExportCompileCommands(llvm::StringRef projectPath, llvm::StringRef cmakeArgs, llvm::StringRef tempDir);

} // mrdocs
} // clang

#endif // MRDOCS_LIB_TOOL_CMAKE_EXECUTION_HPP

