//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Fernando Pelliccioni (fpelliccioni@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "CMakeExecution.hpp"
#include "ExecuteAndWaitWithLogging.hpp"
#include <lib/Support/Path.hpp>
#include <mrdocs/Support/ScopeExit.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/Program.h>
#include <system_error>


namespace mrdocs {

namespace {

Expected<std::string>
getCmakePath()
{
    // `CMAKE_ROOT` is an explicit user override and wins over whatever
    // `cmake` happens to be on the `PATH`. Falling back to `PATH` only
    // when `CMAKE_ROOT` is unset (or doesn't contain a `cmake`) matches
    // the convention used by the rest of the toolchain env vars.
    llvm::ErrorOr<std::string> path = std::make_error_code(std::errc::no_such_file_or_directory);
    if (char const* const cmakeRootPath = std::getenv("CMAKE_ROOT"))
    {
        std::string const cmakeBinPath = files::appendPath(cmakeRootPath, "bin");
        path = llvm::sys::findProgramByName("cmake", {cmakeBinPath, cmakeRootPath});
    }
    if (!path)
    {
        path = llvm::sys::findProgramByName("cmake");
    }
    MRDOCS_CHECK(path, "CMake executable not found");
    std::optional<llvm::StringRef> const redirects[] = {llvm::StringRef(), llvm::StringRef(), llvm::StringRef()};
    std::vector<llvm::StringRef> const args = {*path, "--version"};
    int const result = ExecuteAndWaitWithLogging(*path, args, std::nullopt, redirects);
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
    int const result = ExecuteAndWaitWithLogging(cmakePath, args, std::nullopt, redirects);
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
    int const result = ExecuteAndWaitWithLogging(cmakePath, args, std::nullopt, redirects);
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
    if (cmakeHelpExp)
    {
        std::string const cmakeHelp = *std::move(cmakeHelpExp);
        auto const r = parseCmakeHelpOutput(cmakeHelp);
        if (r)
        {
            return *r;
        }
    }

    Expected<std::string> const cmakeSystemInformationExp = executeCmakeSystemInformation(cmakePath);
    if (cmakeSystemInformationExp)
    {
        std::string const cmakeSystemInformation = *std::move(cmakeSystemInformationExp);
        auto const r = parseCmakeSystemInformationOutput(cmakeSystemInformation);
        if (r)
        {
            return *r;
        }
    }

    if (llvm::sys::path::extension(cmakePath) == ".exe")
    {
        return "Visual Studio 17 2022";
    }
    return "Unix Makefiles";
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

/* Pushes the CMake arguments to the `args` vector, replacing the
 * default generator with Ninja if Visual Studio is the default generator.
 */
// If `value` is a relative path that contains a path separator and
// resolves against `baseDir` to an existing filesystem entry, return
// the absolute path. Otherwise return an empty optional, leaving the
// value alone.
//
// This lets users write `cmake: '-D Boost_DIR=../external/boost/...'`
// in `mrdocs.yml` and have it resolve like the other path-bearing
// fields in the config. CMake itself stores `-D` values verbatim and
// does not resolve them against any predictable directory, so MrDocs
// translates the path before invoking `cmake`.
std::optional<std::string>
resolveRelativePath(
    llvm::StringRef value,
    llvm::StringRef baseDir)
{
    if (baseDir.empty() || value.empty())
    {
        return std::nullopt;
    }
    if (llvm::sys::path::is_absolute(value))
    {
        return std::nullopt;
    }
    // Heuristic: only treat the value as a path if it contains a
    // path separator. This avoids accidentally rewriting things
    // like `-D MY_FLAG=ON` or `-D LEVEL=2`.
    if (value.find('/') == llvm::StringRef::npos &&
        value.find('\\') == llvm::StringRef::npos)
    {
        return std::nullopt;
    }
    llvm::SmallString<256> candidate(baseDir);
    llvm::sys::path::append(candidate, value);
    if (!llvm::sys::fs::exists(candidate))
    {
        return std::nullopt;
    }
    llvm::sys::fs::make_absolute(candidate);
    llvm::sys::path::remove_dots(candidate, /*remove_dot_dot=*/true);
    return candidate.str().str();
}

// Apply `resolveRelativePath` to a single token formatted as
// `NAME=VALUE`, returning the rewritten token or std::nullopt if
// the value didn't qualify.
std::optional<std::string>
maybeRewriteDValue(
    llvm::StringRef nameAndValue,
    llvm::StringRef baseDir)
{
    auto const eq = nameAndValue.find('=');
    if (eq == llvm::StringRef::npos)
    {
        return std::nullopt;
    }
    auto const value = nameAndValue.substr(eq + 1);
    auto const absolute = resolveRelativePath(value, baseDir);
    if (!absolute)
    {
        return std::nullopt;
    }
    std::string rewritten(nameAndValue.substr(0, eq + 1));
    rewritten += *absolute;
    return rewritten;
}

Expected<llvm::SmallVector<llvm::SmallString<128>, 50>>
generateCMakeArgs(
    std::string const& cmakePath,
    llvm::StringRef cmakeArgs,
    llvm::StringRef projectPath,
    llvm::StringRef buildDir,
    llvm::StringRef configDir)
{
    auto const userArgs = parseBashArgs(cmakeArgs.str());
    llvm::SmallVector<llvm::SmallString<128>, 50> res;

    res.emplace_back(cmakePath);
    res.emplace_back("-S");
    res.emplace_back(projectPath);
    res.emplace_back("-B");
    res.emplace_back(buildDir);

    // CMake processes `-D` flags in order: later values win. Insert
    // MrDocs's defaults first, so anything the user puts in the
    // `cmake:` configuration option (or in their CLI `cmakeArgs`)
    // can override them. The two defaults below are non-negotiable
    // for the rest of the pipeline to function, but the user is
    // still free to set them to a different value (for example,
    // `cmake: '-DMRDOCS_BUILD=OFF'` to ask a CMakeLists.txt to
    // expose a target it normally hides during the documentation
    // build).
    res.emplace_back("-D");
    res.emplace_back("MRDOCS_BUILD=ON");
    res.emplace_back("-D");
    res.emplace_back("CMAKE_EXPORT_COMPILE_COMMANDS=ON");

    bool generatorSet = false;
    std::string_view generatorName = {};
    bool visualStudioSet = false;

    for (size_t i = 0; i < userArgs.size(); ++i)
    {
        // Cache variables: forward, rewriting relative path values
        // to absolute against the config directory. The defaults
        // inserted above are positioned before this point, so an
        // explicit user value lands later in the argv and wins.
        if (userArgs[i].starts_with("-D"))
        {
            if (userArgs[i].size() == 2 &&
                i + 1 < userArgs.size())
            {
                res.emplace_back(userArgs[i]);
                if (auto rewritten = maybeRewriteDValue(userArgs[i+1], configDir))
                {
                    res.emplace_back(*rewritten);
                }
                else
                {
                    res.emplace_back(userArgs[i+1]);
                }
                ++i;
            }
            else if (userArgs[i].size() > 2)
            {
                llvm::StringRef const body(userArgs[i].data() + 2, userArgs[i].size() - 2);
                if (auto rewritten = maybeRewriteDValue(body, configDir))
                {
                    std::string out("-D");
                    out += *rewritten;
                    res.emplace_back(out);
                }
                else
                {
                    res.emplace_back(userArgs[i]);
                }
            }
            continue;
        }

        // Build dir or source dir
        if (userArgs[i] == "-B" ||
            userArgs[i] == "-S")
        {
            // The build and source dirs will be set by MrDocs
            // Don't push the argument
            if (i + 1 < userArgs.size())
            {
                ++i;
            }
            continue;
        }

        // Generator
        if (userArgs[i].starts_with("-G"))
        {
            if (userArgs[i].size() == 2 &&
                i + 1 < userArgs.size())
            {
                generatorSet = true;
                generatorName = userArgs[i+1];
                ++i;
            }
            else if (userArgs.size() > 2)
            {
                generatorSet = true;
                generatorName = userArgs[i+1];
                generatorName.remove_prefix(2);
            }
            if (generatorSet)
            {
                visualStudioSet = generatorName.starts_with("Visual Studio");
            }
            if (generatorSet && !visualStudioSet)
            {
                res.emplace_back("-G");
                res.emplace_back(generatorName);
            }
            continue;
        }

        // Other args
        res.emplace_back(userArgs[i]);
    }

    if (visualStudioSet)
    {
        res.emplace_back("-G");
        res.emplace_back("Ninja");
    }
    else if (!generatorSet)
    {
        MRDOCS_TRY(
            bool const cmakeDefaultGeneratorIsVisualStudio,
            cmakeDefaultGeneratorIsVisualStudio(cmakePath));
        if (cmakeDefaultGeneratorIsVisualStudio)
        {
            res.emplace_back("-G");
            res.emplace_back("Ninja");
        }
    }

    return res;
}

} // anonymous namespace

Expected<std::string>
executeCmakeExportCompileCommands(
    llvm::StringRef projectPath,
    llvm::StringRef cmakeArgs,
    llvm::StringRef buildDir,
    llvm::StringRef workingDir)
{
    MRDOCS_CHECK(llvm::sys::fs::exists(projectPath), "Project path does not exist");
    MRDOCS_TRY(auto const cmakePath, getCmakePath());

    constexpr std::array<std::optional<llvm::StringRef>, 3>
        redirects = {std::nullopt, std::nullopt, std::nullopt};
    MRDOCS_TRY(auto args, generateCMakeArgs(cmakePath, cmakeArgs, projectPath, buildDir, workingDir));
    std::vector<llvm::StringRef> argsRef(args.begin(), args.end());

    // Switch to the requested working directory so CMake resolves
    // relative paths in cache assignments (`-D VAR=relpath`) against the
    // location the user expects, then restore the previous CWD on exit.
    llvm::SmallString<256> savedCwd;
    bool cwdChanged = false;
    if (!workingDir.empty())
    {
        if (auto const ec = llvm::sys::fs::current_path(savedCwd))
        {
            return Unexpected(Error(ec.message()));
        }
        if (auto const ec = llvm::sys::fs::set_current_path(workingDir))
        {
            return Unexpected(Error(formatError(
                "Failed to set working directory '{}': {}",
                workingDir.operator std::string_view(),
                ec.message())));
        }
        cwdChanged = true;
    }
    ScopeExit const restoreCwd([&]
    {
        if (cwdChanged)
        {
            (void)llvm::sys::fs::set_current_path(savedCwd);
        }
    });

    int const result = ExecuteAndWaitWithLogging(cmakePath, argsRef, std::nullopt, redirects);
    if (result != 0) {
        return Unexpected(Error("CMake execution failed"));
    }

    llvm::SmallString<128> compileCommandsPath(buildDir);
    llvm::sys::path::append(compileCommandsPath, "compile_commands.json");

    MRDOCS_CHECK(
        llvm::sys::fs::exists(compileCommandsPath),
        "CMake execution failed (no compile_commands.json file generated)");

    return compileCommandsPath.str().str();
}


} // mrdocs

