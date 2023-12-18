//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_TOOL_COMPILER_HPP
#define MRDOCS_TOOL_COMPILER_HPP

#include <optional>
#include <string>
#include <vector>
#include <unordered_map>

#include <clang/Tooling/CompilationDatabase.h>
#include <llvm/ADT/StringRef.h>

namespace clang {
namespace mrdocs {

/**
 * @brief Get the compiler verbose output.
 * 
 * @param compilerPath The compiler path.
 * @return std::optional<std::string> The compiler verbose output.
*/
std::optional<std::string> 
getCompilerVerboseOutput(llvm::StringRef compilerPath);

/**
 * @brief Parse the include paths.
 * 
 * @param compilerOutput The compiler output.
 * @return std::vector<std::string> The include paths.
*/
std::vector<std::string> 
parseIncludePaths(std::string const& compilerOutput);

/**
 * @brief Get the compiler default include dir.
 * 
 * @param compDb The compilation database.
 * @return std::unordered_map<std::string, std::vector<std::string>> The compiler default include dir.
*/
std::unordered_map<std::string, std::vector<std::string>> 
getCompilersDefaultIncludeDir(clang::tooling::CompilationDatabase const& compDb);

} // mrdocs
} // clang

#endif
