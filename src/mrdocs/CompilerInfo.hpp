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
#include <mrdocs/Support/Reporter.hpp>
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
 * @param reporter The reporter that prints the command run.
 * @param sysrootArgs The sysroot flags to pass to the compiler, or empty for none.
 * @return The compiler verbose output.
*/
Optional<std::string>
getCompilerVerboseOutput(
    llvm::StringRef compilerPath,
    report::Reporter& reporter,
    std::vector<std::string> const& sysrootArgs = {});

/**
 * @brief Get the sysroot flags of a compile command.
 *
 * Reads the last `-isysroot` or `--sysroot` of the command line, whichever
 * spelling it has, and returns it as `-isysroot <dir>` or `--sysroot=<dir>`
 * with the directory absolute.
 *
 * @param commandLine The compile command, compiler first.
 * @param directory The command's directory, which a relative sysroot is resolved against.
 * @param workingDir The directory a relative `directory` is resolved against.
 * @return The flags, or empty when the command has no sysroot.
*/
std::vector<std::string>
findSysrootArgs(
    std::vector<std::string> const& commandLine,
    llvm::StringRef directory,
    llvm::StringRef workingDir);

/**
 * @brief Find the executable a compile command's compiler names.
 *
 * A bare name (no directory separator) is looked up on the `PATH`, as the
 * driver would run it. A path with a separator is resolved against the
 * command's directory and must exist.
 *
 * @param compiler The compiler as written in the compile command.
 * @param directory The command's directory, which a relative path is resolved against.
 * @param workingDir The directory a relative `directory` is resolved against.
 * @return The absolute path of the compiler, or nothing when it is not found.
*/
Optional<std::string>
resolveCompilerPath(
    llvm::StringRef compiler,
    llvm::StringRef directory,
    llvm::StringRef workingDir);

/**
 * @brief Get the key of the include dirs probed for a compiler and sysroot.
 *
 * @param compilerPath The compiler path.
 * @param sysrootArgs The flags returned by `findSysrootArgs`.
 * @return The key in the map `getCompilersDefaultIncludeDir` returns.
*/
std::string
implicitIncludeKey(
    llvm::StringRef compilerPath,
    std::vector<std::string> const& sysrootArgs);

/**
 * @brief Parse the include paths.
 * 
 * @param compilerOutput The compiler output.
 * @return std::vector<std::string> The include paths.
*/
std::vector<std::string> 
parseIncludePaths(std::string const& compilerOutput);

/**
 * @brief Get the compilers to probe when a compile command's compiler is not found.
 *
 * The compiler the user chose comes first (`CXX`, then `CC`), followed by
 * common compiler names looked up on the `PATH`.
 *
 * @param cxx The value of `CXX`, or empty when unset.
 * @param cc The value of `CC`, or empty when unset.
 * @return The candidates in probe order: `cxx`, `cc`, then common names.
*/
std::vector<std::string>
fallbackCompilerCandidates(
    llvm::StringRef cxx,
    llvm::StringRef cc);

/**
 * @brief Get the compiler default include dir.
 *
 * The probed search list mixes the C++ standard library, the compiler's
 * resource headers, and the C library; each flag keeps its own slice, so
 * the system C library can be used together with the bundled C++ stdlib
 * and vice versa.
 *
 * @param compDb The compilation database.
 * @param workingDir The directory the relative directories of the commands are resolved against.
 * @param useSystemStdlib True to keep the system C++ standard library dirs.
 * @param useSystemLibc True to keep the system C library dirs.
 * @param reporter The reporter that prints each compiler command run.
 * @return The include dirs of each compiler and sysroot, keyed by `implicitIncludeKey`.
*/
std::unordered_map<std::string, std::vector<std::string>>
getCompilersDefaultIncludeDir(
    clang::tooling::CompilationDatabase const& compDb,
    llvm::StringRef workingDir,
    bool useSystemStdlib,
    bool useSystemLibc,
    report::Reporter& reporter);

} // mrdocs


#endif // MRDOCS_LIB_COMPILERINFO_HPP
