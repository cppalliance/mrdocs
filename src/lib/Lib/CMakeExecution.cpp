//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Fernando Pelliccioni (fpelliccioni@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "lib/Lib/CMakeExecution.hpp"

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/Program.h>

namespace clang {
namespace mrdocs {

Expected<std::string>
getCmakePath() {
    auto const path = llvm::sys::findProgramByName("cmake");
    MRDOCS_CHECK(path, "CMake executable not found");
    std::optional<llvm::StringRef> const redirects[] = {llvm::StringRef(), llvm::StringRef(), llvm::StringRef()};
    std::vector<llvm::StringRef> const args = {*path, "--version"};
    int const result = llvm::sys::ExecuteAndWait(*path, args, std::nullopt, redirects);
    MRDOCS_CHECK(result == 0, "CMake execution failed");
    return *path;
}

Expected<std::string>
executeCmakeHelp(llvm::StringRef cmakePath)
{
    llvm::SmallString<128> outputPath;
    MRDOCS_CHECK(!llvm::sys::fs::createTemporaryFile("cmake-help", "txt", outputPath), 
        "Failed to create temporary file");
    std::optional<llvm::StringRef> const redirects[] = {llvm::StringRef(), outputPath.str(), llvm::StringRef()};
    std::vector<llvm::StringRef> const args = {cmakePath, "--help"};
    llvm::ArrayRef<llvm::StringRef> emptyEnv;
    int const result = llvm::sys::ExecuteAndWait(cmakePath, args, emptyEnv, redirects);
    MRDOCS_CHECK(result == 0, "CMake execution failed");

    auto const bufferOrError = llvm::MemoryBuffer::getFile(outputPath);
    MRDOCS_CHECK(bufferOrError, "Failed to read CMake help output");

    return bufferOrError.get()->getBuffer().str();
}

Expected<std::string>
getCmakeDefaultGenerator(llvm::StringRef cmakePath) 
{
    MRDOCS_TRY(auto const cmakeHelp, executeCmakeHelp(cmakePath));

    std::istringstream stream(cmakeHelp);
    std::string line;
    std::string defaultGenerator;

    while (std::getline(stream, line)) {
        if (line[0] == '*' && line[1] == ' ') {
            size_t const start = 2;
            size_t const end = line.find("=", start);
            if (end == std::string::npos) {
                continue;
            }
            return line.substr(start, end - start);
        }
    }
    return Unexpected(Error("Default CMake generator not found"));
}

Expected<bool>
cmakeDefaultGeneratorIsVisualStudio(llvm::StringRef cmakePath) 
{
    MRDOCS_TRY(auto const defaultGenerator, getCmakeDefaultGenerator(cmakePath));
    return defaultGenerator.find("Visual Studio") != std::string::npos;
}

Expected<std::string>
executeCmakeExportCompileCommands(llvm::StringRef projectPath) 
{
    MRDOCS_TRY(auto const cmakePath, getCmakePath());
    MRDOCS_CHECK(llvm::sys::fs::exists(projectPath), "CMakeLists.txt not found");
    MRDOCS_TRY(auto const cmakeDefaultGeneratorIsVisualStudio, cmakeDefaultGeneratorIsVisualStudio(cmakePath));

    llvm::SmallString<128> tempDir;
    MRDOCS_CHECK(!llvm::sys::fs::createUniqueDirectory("compile_commands", tempDir), "Failed to create temporary directory");

    std::optional<llvm::StringRef> const redirects[] = {llvm::StringRef(), llvm::StringRef(), llvm::StringRef()};
    std::vector<llvm::StringRef> args = {cmakePath, "-S", projectPath, "-B", tempDir.str(), "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"};
    if (cmakeDefaultGeneratorIsVisualStudio) {
        args.push_back("-G");
        args.push_back("Ninja");
    }
        
    int const result = llvm::sys::ExecuteAndWait(cmakePath, args, std::nullopt, redirects);
    MRDOCS_CHECK(result == 0, "CMake execution failed");

    llvm::SmallString<128> compileCommandsPath(tempDir);
    llvm::sys::path::append(compileCommandsPath, "compile_commands.json");

    return compileCommandsPath.str().str();
}

} // mrdocs
} // clang
