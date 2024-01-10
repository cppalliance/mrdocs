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

namespace {

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
    return defaultGenerator.starts_with("Visual Studio");
}

std::vector<std::string>
parseCmakeArgs(std::string const& cmakeArgsStr) 
{
    std::vector<std::string> parsedArgs;
    std::string arg;
    bool inQuotes = false;
    bool escapeNextChar = false;

    for (char ch : cmakeArgsStr) 
    {
        if (escapeNextChar) 
        {
            arg += ch;
            escapeNextChar = false;
        } 
        else if (ch == '\\') 
        {
            escapeNextChar = true;
        } 
        else if (ch == '"') 
        {
            inQuotes = !inQuotes;
        } 
        else if (std::isspace(ch) && !inQuotes) 
        {
            if ( ! arg.empty()) 
            {
                if (arg[0] == '-' && arg.size() == 2)
                    continue;
                parsedArgs.push_back(arg);
                arg.clear();
            }
        } 
        else 
        {
            arg += ch;
        }
    }

    if ( ! arg.empty()) 
    {
        parsedArgs.push_back(arg);
    }

    return parsedArgs;
}

} // anonymous namespace

Expected<std::string>
executeCmakeExportCompileCommands(llvm::StringRef projectPath, llvm::StringRef cmakeArgs) 
{
    MRDOCS_CHECK(llvm::sys::fs::exists(projectPath), "Project path does not exist");
    MRDOCS_TRY(auto const cmakePath, getCmakePath());

    llvm::SmallString<128> tempDir;
    MRDOCS_CHECK(!llvm::sys::fs::createUniqueDirectory("compile_commands", tempDir), "Failed to create temporary directory");

    std::optional<llvm::StringRef> const redirects[] = {llvm::StringRef(), llvm::StringRef(), llvm::StringRef()};
    std::vector<llvm::StringRef> args = {cmakePath, "-S", projectPath, "-B", tempDir.str(), "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"};

    auto const additionalArgs = parseCmakeArgs(cmakeArgs.str());
    bool visualStudioFound = false;
    for (auto const& arg : additionalArgs) 
    {
        if (arg.starts_with("-G") && arg.find("Visual Studio", 2) != std::string::npos)
        {
            args.push_back("-GNinja");
            visualStudioFound = true;
            continue;
        }        
        if (arg.starts_with("-D") && arg.find("CMAKE_EXPORT_COMPILE_COMMANDS", 2) != std::string::npos)
        {
            continue;
        }         
        args.push_back(arg);
    }

    if ( ! visualStudioFound) 
    {
        MRDOCS_TRY(auto const cmakeDefaultGeneratorIsVisualStudio, cmakeDefaultGeneratorIsVisualStudio(cmakePath));
        if (cmakeDefaultGeneratorIsVisualStudio) 
        {
            args.push_back("-GNinja");
        }
    }

    int const result = llvm::sys::ExecuteAndWait(cmakePath, args, std::nullopt, redirects);
    MRDOCS_CHECK(result == 0, "CMake execution failed");

    llvm::SmallString<128> compileCommandsPath(tempDir);
    llvm::sys::path::append(compileCommandsPath, "compile_commands.json");

    return compileCommandsPath.str().str();
}

} // mrdocs
} // clang
