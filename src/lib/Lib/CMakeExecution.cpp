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
#include "lib/Support/Path.hpp"

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/Program.h>

namespace clang {
namespace mrdocs {

namespace {

Expected<std::string>
getCmakePath()
{
    llvm::ErrorOr<std::string> const path = llvm::sys::findProgramByName("cmake");
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
    ScopedTempFile const outputPath("cmake-help", "txt");
    MRDOCS_CHECK(outputPath, "Failed to create temporary file");
    ScopedTempFile const errOutputPath("cmake-help-err", "txt");
    MRDOCS_CHECK(errOutputPath, "Failed to create temporary file");
    std::optional<llvm::StringRef> const redirects[] = {llvm::StringRef(), outputPath.path(), errOutputPath.path()};
    std::vector<llvm::StringRef> const args = {cmakePath, "--help"};
    int const result = llvm::sys::ExecuteAndWait(cmakePath, args, std::nullopt, redirects);
    if (result != 0)
    {
        auto const bufferOrError = llvm::MemoryBuffer::getFile(errOutputPath.path());
        MRDOCS_CHECK(bufferOrError, "CMake --help execution failed (no error output available)");
        auto const bufferOrError2 = llvm::MemoryBuffer::getFile(errOutputPath.path());
        MRDOCS_CHECK(bufferOrError2, "CMake --help execution failed (no error output available)");
        // Concatenate both outputs
        std::string output;
        if (bufferOrError.get()->getBuffer().str().empty())
        {
            output = bufferOrError2.get()->getBuffer().str();
        }
        else if (bufferOrError2.get()->getBuffer().str().empty())
        {
            output = bufferOrError.get()->getBuffer().str();
        }
        else
        {
            output = bufferOrError.get()->getBuffer().str() + "\n" + bufferOrError2.get()->getBuffer().str();
        }
        return Unexpected(Error("CMake --help execution failed: \n" + output));
    }
    auto const bufferOrError = llvm::MemoryBuffer::getFile(outputPath.path());
    MRDOCS_CHECK(bufferOrError, "Failed to read CMake --help output");
    return bufferOrError.get()->getBuffer().str();
}


Expected<std::string>
executeCmakeSystemInformation(llvm::StringRef cmakePath)
{
    ScopedTempFile const outputPath("cmake-system-information-out", "txt");
    MRDOCS_CHECK(outputPath, "Failed to create temporary file");
    ScopedTempFile const errOutputPath("cmake-system-information-err", "txt");
    MRDOCS_CHECK(errOutputPath, "Failed to create temporary file");
    std::optional<llvm::StringRef> const redirects[] = {llvm::StringRef(), outputPath.path(), errOutputPath.path()};
    std::vector<llvm::StringRef> const args = {cmakePath, "--system-information"};
    int const result = llvm::sys::ExecuteAndWait(cmakePath, args, std::nullopt, redirects);
    if (result != 0)
    {
        auto const bufferOrError = llvm::MemoryBuffer::getFile(errOutputPath.path());
        MRDOCS_CHECK(bufferOrError, "CMake --system-information execution failed (no error output available)");
        auto const bufferOrError2 = llvm::MemoryBuffer::getFile(errOutputPath.path());
        MRDOCS_CHECK(bufferOrError2, "CMake --system-information execution failed (no error output available)");
        // Concatenate both outputs
        std::string output;
        if (bufferOrError.get()->getBuffer().str().empty())
        {
            output = bufferOrError2.get()->getBuffer().str();
        }
        else if (bufferOrError2.get()->getBuffer().str().empty())
        {
            output = bufferOrError.get()->getBuffer().str();
        }
        else
        {
            output = bufferOrError.get()->getBuffer().str() + "\n" + bufferOrError2.get()->getBuffer().str();
        }
        return Unexpected(Error("CMake --help execution failed: \n" + output));
    }
    auto const bufferOrError = llvm::MemoryBuffer::getFile(outputPath.path());
    MRDOCS_CHECK(bufferOrError, "Failed to read CMake --system-information output");
    return bufferOrError.get()->getBuffer().str();
}

Expected<std::string>
parseCmakeHelpOutput(std::string const& cmakeHelp)
{
    std::istringstream stream(cmakeHelp);
    std::string line;
    std::string defaultGenerator;

    while (std::getline(stream, line))
    {
        if (line[0] == '*' && line[1] == ' ')
        {
            size_t const start = 2;
            size_t const end = line.find("=", start);
            if (end == std::string::npos)
            {
                continue;
            }
            return line.substr(start, end - start);
        }
    }
    return Unexpected(Error("Default CMake generator not found"));
}

Expected<std::string>
parseCmakeSystemInformationOutput(std::string const& cmakeSystemInformation)
{
    std::istringstream stream(cmakeSystemInformation);
    std::string line;
    std::string defaultGenerator;

    while (std::getline(stream, line))
    {
        if (line.starts_with("CMAKE_GENERATOR \""))
        {
            size_t const start = 17;
            size_t const end = line.find("\"", start);
            if (end == std::string::npos)
            {
                continue;
            }
            return line.substr(start, end - start);
        }
    }
    return Unexpected(Error("Default CMake generator not found"));
}

Expected<std::string>
getCmakeDefaultGenerator(llvm::StringRef cmakePath)
{
    Expected<std::string> const cmakeHelpExp = executeCmakeHelp(cmakePath);
    if (!cmakeHelpExp)
    {
        Expected<std::string> const cmakeSystemInformationExp = executeCmakeSystemInformation(cmakePath);
        if (!cmakeSystemInformationExp)
        {
            if (llvm::sys::path::extension(cmakePath) == ".exe")
            {
                return "Visual Studio 17 2022";
            }
            return "Unix Makefiles";
        }
        std::string const cmakeSystemInformation = *std::move(cmakeSystemInformationExp);
        return parseCmakeSystemInformationOutput(cmakeSystemInformation);
    }

    std::string const cmakeHelp = *std::move(cmakeHelpExp);
    return parseCmakeHelpOutput(cmakeHelp);
}

Expected<bool>
cmakeDefaultGeneratorIsVisualStudio(llvm::StringRef cmakePath)
{
    MRDOCS_TRY(auto const defaultGenerator, getCmakeDefaultGenerator(cmakePath));
    return defaultGenerator.starts_with("Visual Studio");
}

Expected<std::string_view>
parseBashIdentifier(std::string_view str)
{
    if (str.empty())
    {
        return Unexpected(Error("Empty argument"));
    }
    if (str[0] != '$')
    {
        return Unexpected(Error("Argument does not start with '$'"));
    }
    if (str.size() == 1)
    {
        return Unexpected(Error("Argument does not contain identifier"));
    }
    // Check if first char matches [a-zA-Z_]
    if (str[1] != '_' && (str[1] < 'a' || str[1] > 'z') && (str[1] < 'A' || str[1] > 'Z'))
    {
        return Unexpected(Error("Argument does not start with [a-zA-Z_]"));
    }
    // Iterate other chars including valid chars in identifier
    std::string_view identifier = str.substr(1, 1);
    for (size_t i = 2; i < str.size(); ++i)
    {
        char const ch = str[i];
        if (ch != '_' && (ch < 'a' || ch > 'z') && (ch < 'A' || ch > 'Z') && (ch < '0' || ch > '9'))
        {
            break;
        }
        identifier = str.substr(1, i);
    }
    return identifier;
}

std::vector<std::string>
parseBashArgs(std::string_view str)
{
    std::vector<std::string> args;
    char curQuote = '\0';
    std::string curArg;

    for (std::size_t i = 0; i < str.size(); ++i)
    {
        char const c = str[i];
        bool const inQuote = curQuote != '\0';
        bool const curIsQuote = c == '\'' || c == '"';
        bool const curIsEscaped = i > 0 && str[i - 1] == '\\';
        if (!inQuote)
        {
            if (!curIsEscaped)
            {
                if (curIsQuote)
                {
                    // Open quotes
                    curQuote = c;
                }
                else if (c == ' ')
                {
                    // End of argument
                    if (!curArg.empty())
                    {
                        args.push_back(curArg);
                        curArg.clear();
                    }
                }
                else if (c == '$')
                {
                    // Expand environment variable
                    Expected<std::string_view> id =
                        parseBashIdentifier(str.substr(i));
                    if (id)
                    {
                        std::string idStr(*id);
                        char const* const value = std::getenv(idStr.c_str());
                        if (value == nullptr)
                        {
                            curArg += c;
                        }
                        else
                        {
                            curArg += value;
                            i += idStr.size();
                        }
                    }
                    else
                    {
                        curArg += c;
                    }
                }
                else if (c != '\\')
                {
                    // Add character to current argument
                    curArg += c;
                }
            }
            else
            {
                // Current character is escaped:
                // add whatever it is to current argument
                curArg += c;
            }
        }
        else if (curQuote == '\"')
        {
            // In \" quotes:
            // Preserve the literal value of all characters except for
            // ($), (`), ("), (\), and the (!) character
            if (!curIsEscaped)
            {
                if (c == curQuote)
                {
                    // Close quotes
                    curQuote = '\0';
                }
                else if (c == '$')
                {
                    // Expand environment variable
                    Expected<std::string_view> id =
                        parseBashIdentifier(str.substr(i));
                    if (id)
                    {
                        std::string idStr(*id);
                        char const* const value = std::getenv(idStr.c_str());
                        if (value == nullptr)
                        {
                            curArg += c;
                        }
                        else
                        {
                            curArg += value;
                            i += idStr.size();
                        }
                    }
                    else
                    {
                        curArg += c;
                    }
                }
                else if (c != '\\')
                {
                    // Add character to current argument
                    curArg += c;
                }
            }
            else
            {
                // Current character is escaped:
                // add whatever it is to current argument
                // Chars that don't need escaping also include the slash
                if (c != '$' && c != '`' && c != '"' && c != '\\')
                {
                    curArg += '\\';
                }
                curArg += c;
            }
        }
        else if (curQuote == '\'')
        {
            // In \' quotes:
            // Preserve the literal value of each character within the
            // quotes
            if (c != curQuote)
            {
                // Add character to current argument
                curArg += c;
            }
            else
            {
                // Close quotes
                curQuote = '\0';
            }
        }
    }
    // Add last argument
    if (!curArg.empty())
    {
        args.push_back(curArg);
    }
    return args;
}

Expected<void>
pushCMakeArgs(
    std::string const& cmakePath,
    std::vector<llvm::StringRef> &args,
    std::vector<std::string> const& additionalArgs) {
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
                        args.emplace_back("-GNinja");
                        visualStudioFound = true;
                        ++i;
                        continue;
                    }
                }
            } else {
                if (arg.find("Visual Studio", 2) != std::string::npos)
                {
                    args.emplace_back("-GNinja");
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
        args.emplace_back(arg);
    }

    if (!visualStudioFound)
    {
        MRDOCS_TRY(
            bool const cmakeDefaultGeneratorIsVisualStudio,
            cmakeDefaultGeneratorIsVisualStudio(cmakePath));
        if (cmakeDefaultGeneratorIsVisualStudio)
        {
            args.emplace_back("-GNinja");
        }
    }
    return {};
}

} // anonymous namespace

Expected<std::string>
executeCmakeExportCompileCommands(llvm::StringRef projectPath, llvm::StringRef cmakeArgs, llvm::StringRef tempDir)
{
    MRDOCS_CHECK(llvm::sys::fs::exists(projectPath), "Project path does not exist");
    MRDOCS_TRY(auto const cmakePath, getCmakePath());

    ScopedTempFile const errorPath("cmake-error", "txt");
    MRDOCS_CHECK(errorPath, "Failed to create temporary file");

    std::optional<llvm::StringRef> const redirects[] = {llvm::StringRef(), llvm::StringRef(), errorPath.path()};
    std::vector<llvm::StringRef> args = {cmakePath, "-S", projectPath, "-B", tempDir, "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"};

    auto const additionalArgs = parseBashArgs(cmakeArgs.str());
    MRDOCS_TRY(pushCMakeArgs(cmakePath, args, additionalArgs));

    int const result = llvm::sys::ExecuteAndWait(cmakePath, args, std::nullopt, redirects);
    if (result != 0) {
        auto bufferOrError = llvm::MemoryBuffer::getFile(errorPath.path());
        MRDOCS_CHECK(bufferOrError, "CMake execution failed (no error output available)");
        return Unexpected(Error("CMake execution failed: \n" + bufferOrError.get()->getBuffer().str()));
    }

    llvm::SmallString<128> compileCommandsPath(tempDir);
    llvm::sys::path::append(compileCommandsPath, "compile_commands.json");

    MRDOCS_CHECK(
        llvm::sys::fs::exists(compileCommandsPath),
        "CMake execution failed (no compile_commands.json file generated)");

    return compileCommandsPath.str().str();
}


} // mrdocs
} // clang
