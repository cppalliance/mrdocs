//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_COMPILERINFO_HPP
#define MRDOCS_LIB_COMPILERINFO_HPP

#include <mrdocs/ADT/Optional.hpp>
#include <clang/Tooling/CompilationDatabase.h>
#include <llvm/ADT/StringRef.h>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>


namespace mrdocs {

/**
 * @brief Get the compiler verbose output.
 * 
 * @param compilerPath The compiler path.
 * @return The compiler verbose output.
*/
Optional<std::string>
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
 * The probed search list mixes the C++ standard library, the compiler's
 * resource headers, and the C library; each flag keeps its own slice, so
 * the system C library can be used together with the bundled C++ stdlib
 * and vice versa.
 *
 * @param compDb The compilation database.
 * @param useSystemStdlib True to keep the system C++ standard library dirs.
 * @param useSystemLibc True to keep the system C library dirs.
 * @return std::unordered_map<std::string, std::vector<std::string>> The compiler default include dir.
*/
std::unordered_map<std::string, std::vector<std::string>>
getCompilersDefaultIncludeDir(
    clang::tooling::CompilationDatabase const& compDb,
    bool useSystemStdlib,
    bool useSystemLibc);

} // mrdocs


#endif // MRDOCS_LIB_COMPILERINFO_HPP
