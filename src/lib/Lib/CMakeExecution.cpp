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
    MRDOCS_CHECK(result == 0, "CMake execution failed when checking version");
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
    MRDOCS_CHECK(result == 0, "CMake execution failed when trying to get help");

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
parseCmakeArgs(std::string const& cmakeArgsStr) {
    std::vector<std::string> args;
    std::string currentArg;
    char quoteChar = '\0';
    bool escapeNextChar = false;

    for (char ch : cmakeArgsStr) 
    {
        if (escapeNextChar) 
        {
            currentArg += ch;
            escapeNextChar = false;
        } 
        else if (ch == '\\') 
        {
            escapeNextChar = true;
        } 
        else if ((ch == '"' || ch == '\'')) 
        {
            if (quoteChar == '\0') 
            {
                quoteChar = ch;
            } 
            else if (ch == quoteChar) 
            {
                quoteChar = '\0';
            } 
            else 
            {
                currentArg.push_back(ch);
            }
        } else if (std::isspace(ch)) 
        {
            if (quoteChar != '\0') 
            {
                currentArg.push_back(ch);
            } 
            else 
            {
                if ( ! currentArg.empty()) 
                {
                    args.push_back(currentArg);
                    currentArg.clear();
                }
            }            
        } else 
        {
            currentArg += ch;
        }
    }

    if ( ! currentArg.empty()) 
    {
        args.push_back(currentArg);
    }

    return args;
}

} // anonymous namespace

Expected<std::string>
executeCmakeExportCompileCommands(llvm::StringRef projectPath, llvm::StringRef cmakeArgs) 
{
    MRDOCS_CHECK(llvm::sys::fs::exists(projectPath), "Project path does not exist");
    MRDOCS_TRY(auto const cmakePath, getCmakePath());

    llvm::SmallString<128> tempDir;
    MRDOCS_CHECK(!llvm::sys::fs::createUniqueDirectory("compile_commands", tempDir), "Failed to create temporary directory");

    llvm::SmallString<128> errorPath;
    MRDOCS_CHECK(!llvm::sys::fs::createTemporaryFile("cmake-error", "txt", errorPath), 
        "Failed to create temporary file");        

    std::optional<llvm::StringRef> const redirects[] = {llvm::StringRef(), llvm::StringRef(), errorPath.str()};
    std::vector<llvm::StringRef> args = {cmakePath, "-S", projectPath, "-B", tempDir.str(), "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"};

    auto const additionalArgs = parseCmakeArgs(cmakeArgs.str());
    
    bool visualStudioFound = false;
    for (size_t i = 0; i < additionalArgs.size(); ++i) 
    {
        auto const& arg = additionalArgs[i];
        if (arg.starts_with("-G"))
        {
            if (arg.size() == 2) 
            {
                if (i + 1 < additionalArgs.size()) 
                {
                    auto const& generatorName = additionalArgs[i + 1];
                    if (generatorName.starts_with("Visual Studio")) 
                    {
                        args.push_back("-GNinja");
                        visualStudioFound = true;
                        ++i;
                        continue;
                    }
                }
            } else {
                if (arg.find("Visual Studio", 2) != std::string::npos) 
                {
                    args.push_back("-GNinja");
                    visualStudioFound = true;
                    continue;
                }
            }
        }

        if (arg.starts_with("-D"))
        {
            if (arg.size() == 2) 
            {
                if (i + 1 < additionalArgs.size()) 
                {
                    auto const& optionName = additionalArgs[i + 1];
                    if (optionName.starts_with("CMAKE_EXPORT_COMPILE_COMMANDS")) 
                    {
                        ++i;
                        continue;
                    }
                }
            } else {
                if (arg.find("CMAKE_EXPORT_COMPILE_COMMANDS", 2) != std::string::npos) 
                {
                    continue;
                }
            }
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
    if (result != 0) {
        auto bufferOrError = llvm::MemoryBuffer::getFile(errorPath);
        MRDOCS_CHECK(bufferOrError, "CMake execution failed (no error output available)");
        return Unexpected(Error("CMake execution failed: \n" + bufferOrError.get()->getBuffer().str()));
    }

    llvm::SmallString<128> compileCommandsPath(tempDir);
    llvm::sys::path::append(compileCommandsPath, "compile_commands.json");

    return compileCommandsPath.str().str();
}

} // mrdocs
} // clang
