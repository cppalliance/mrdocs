//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "CompilationDatabaseBuilder.hpp"
#include "CompilerInfo.hpp"
#include "MrDocsSettingsDB.hpp"
#include "Support/CMakeExecution.hpp"
#include "Support/Filesystem/Temp.hpp"
#include <mrdocs/Support/Container/Algorithm.hpp>
#include <mrdocs/Support/Report.hpp>
#include <llvm/Support/MemoryBuffer.h>
#include <optional>


namespace mrdocs {

namespace {

/** Run CMake to configure a project and export its compile_commands.json.

    The input is a `CMakeLists.txt` file, as resolved by
    @ref resolveCompilationDatabasePath. CMake configures its parent
    directory into `buildDir` and writes a `compile_commands.json`, whose
    path is returned. Reading an existing `compile_commands.json` is a
    separate strategy, not handled here.

    @param cmakeListsPath A `CMakeLists.txt` file.
    @param cmakeArgs Extra arguments passed to the CMake invocation.
    @param buildDir The directory CMake configures into.
    @param workingDir The working directory for the CMake invocation.
    @return The path to the generated `compile_commands.json`, or an error.
*/
Expected<std::string>
generateCompileCommandsFileWithCmake(
    llvm::StringRef cmakeListsPath,
    llvm::StringRef cmakeArgs,
    llvm::StringRef buildDir,
    llvm::StringRef workingDir)
{
    MRDOCS_ASSERT(files::getFileName(cmakeListsPath) == "CMakeLists.txt");
    std::string const cmakeSourceDir(files::getParentDir(cmakeListsPath));
    return executeCmakeExportCompileCommands(
        cmakeSourceDir, cmakeArgs, buildDir, workingDir);
}

/** Determine which path describes the compilation database.

    Returns the configured `compilation-database`, or, when none is set but
    a `cmake` invocation is configured, a `CMakeLists.txt` in the source
    root. An empty result means the database must be synthesized from the
    configuration. The chosen path is validated before it is returned, so a
    bad path surfaces here with a clear message.
*/
Expected<std::string>
resolveCompilationDatabasePath(Config const& settings)
{
    // Case 1: If no path is configured, so the database will be synthesized from the
    // configuration. This is valid, so return success with an empty path.
    if (settings.compilationDatabase.empty() &&
        settings.cmake.empty())
    {
        return {};
    }

    // Look for explicit parameter
    // Case 2: the explicit config options are: CMakeLists.txt,
    // compile_commands.json, or a directory containing either of those.
    std::string dbPath;
    if (!settings.compilationDatabase.empty())
    {
        dbPath = settings.compilationDatabase;
        MRDOCS_CHECK(
            files::exists(dbPath),
            formatError("Compilation database path does not exist: '{}'", dbPath));
        auto validFilenames = { "compile_commands.json", "CMakeLists.txt" };
        if (files::isDirectory(dbPath))
        {
            if (auto it = std::ranges::find_if(
                validFilenames,
                [&](auto const& name) {
                return files::exists(files::appendPath(dbPath, name));
            }); it != validFilenames.end())
            {
                dbPath = files::appendPath(dbPath, *it);
            }
            else
            {
                return Unexpected(formatError(
                    "Compilation database directory must contain either a "
                    "compile_commands.json or a CMakeLists.txt: '{}'",
                    dbPath));
            }
        }
        auto filename = files::getFileName(dbPath);
        MRDOCS_CHECK_OR(
            is_one_of(filename, { "compile_commands.json", "CMakeLists.txt" }),
            Unexpected(formatError(
                "Invalid compilation database path: '{}'", dbPath)));
        MRDOCS_CHECK_OR(files::isRegularFile(dbPath),
            Unexpected(formatError(
                "Compilation database must be a regular file: '{}'",
                dbPath)));
        return dbPath;
    }

    // If the db it unset but we have cmake commands, we can infer the
    // CMakeLists.txt from the source root was the intended behavior.
    if (!settings.cmake.empty())
    {
        std::string const cmakeLists =
            files::appendPath(settings.sourceRoot, "CMakeLists.txt");
        if (files::exists(cmakeLists))
        {
            return cmakeLists;
        }
    }

    return Unexpected(formatError(
        "No compilation database path configured, and cannot infer one from "
        "the configuration. Please set `compilation-database` to a valid path, "
        "or configure CMake with a valid `source-root` and `cmake` command."));
}

/** Synthesize a compilation database from the configuration alone.

    Used when no compilation-database path was given or inferred: the
    compile commands are derived from `source-root`, includes, defines, and
    the standard-library settings.
*/
MrDocsCompilationDatabase
synthesizeCompilationDatabase(
    Config const& config)
{
    MrDocsSettingsDB settingsDB{config};
    auto const defaultIncludePaths = getCompilersDefaultIncludeDir(
        settingsDB, config.useSystemStdlib, config.useSystemLibc);
    return MrDocsCompilationDatabase(
        config.sourceRoot,
        settingsDB,
        config,
        defaultIncludePaths);
}

/** Where CMake builds while generating the database, plus its owner.

    An explicit `cmake-build-dir` is used as given and never removed.
    Otherwise a scratch directory is created and returned in `scratch` so
    the caller can tie its lifetime to the database it backs.
*/
struct BuildLocation
{
    std::string buildPath;
    std::optional<ScopedTempDirectory> scratch;
};

Expected<BuildLocation>
resolveCMakeBuildLocation(Config const& settings)
{
    if (!settings.cmakeBuildDir.empty())
    {
        MRDOCS_TRY(files::createDirectory(settings.cmakeBuildDir));
        return BuildLocation{settings.cmakeBuildDir, std::nullopt};
    }
    std::string const outDir = files::looksLikeDirectory(settings.output)
        ? settings.output
        : std::string(files::getParentDir(settings.output));
    MRDOCS_TRY(
        ScopedTempDirectory scratch,
        makeScratchDirectory(
            "cmake-build",
            { outDir, std::string(settings.configDir()) }));
    std::string buildPath = files::appendPath(scratch.path(), "build");
    return BuildLocation{std::move(buildPath), std::move(scratch)};
}

/** Read a compile_commands.json file, substituting `${MRDOCS_SOURCE_ROOT}`.

    The standard `compile_commands.json` format requires absolute paths in
    the `directory` field and in `file`/`command`/`arguments`, which makes
    hand-written databases unportable across machines. Resolving
    `${MRDOCS_SOURCE_ROOT}` against the project's `source-root` lets authors
    check a manually written database into version control alongside their
    headers. The token is namespaced with `MRDOCS_` so it cannot appear in a
    legitimate compiler argument by accident; add more placeholders under
    the same `${MRDOCS_*}` namespace.
*/
Expected<std::string>
readCompileCommands(
    llvm::StringRef path,
    std::string_view sourceRoot)
{
    auto buf = llvm::MemoryBuffer::getFile(path);
    if (!buf)
    {
        // Format a std::string_view, not the llvm::StringRef: std::format
        // would otherwise pick the C++23 range formatter for StringRef
        // (rendering it as a char list) in a TU that lacks the custom
        // formatter, which conflicts across TUs.
        return Unexpected(formatError(
            "Failed to read compilation database `{}`: {}",
            std::string_view(path),
            buf.getError().message()));
    }
    std::string content = (*buf)->getBuffer().str();
    static constexpr std::string_view placeholder = "${MRDOCS_SOURCE_ROOT}";
    size_t pos = 0;
    while ((pos = content.find(placeholder, pos)) != std::string::npos)
    {
        content.replace(pos, placeholder.size(), sourceRoot);
        pos += sourceRoot.size();
    }
    return content;
}

/** Read an existing compile_commands.json file into a database.

    This is the strategy for a configuration that already points at a
    `compile_commands.json`: the file is read and parsed directly, with no
    CMake involved. It is also reused by the CMake strategy to load the file
    CMake produces.
*/
Expected<MrDocsCompilationDatabase>
readCompilationDatabase(
    Config const& config,
    std::string compileCommandsPath)
{
    compileCommandsPath = files::normalizePath(compileCommandsPath);
    MRDOCS_TRY(compileCommandsPath, files::makeAbsolute(compileCommandsPath));

    MRDOCS_TRY(
        std::string content,
        readCompileCommands(compileCommandsPath, config.sourceRoot));

    std::string errorMessage;
    std::unique_ptr<clang::tooling::JSONCompilationDatabase> jsonDatabase =
        clang::tooling::JSONCompilationDatabase::loadFromBuffer(
            content,
            errorMessage,
            clang::tooling::JSONCommandLineSyntax::AutoDetect);
    if (!jsonDatabase)
    {
        return Unexpected(formatError(
            "Failed to load compilation database: {}", errorMessage));
    }

    auto const defaultIncludePaths = getCompilersDefaultIncludeDir(
        *jsonDatabase, config.useSystemStdlib, config.useSystemLibc);
    return MrDocsCompilationDatabase(
        files::getParentDir(compileCommandsPath),
        *jsonDatabase,
        config,
        defaultIncludePaths);
}

/** Generate a database by running CMake, then read it.

    This is the strategy for a configuration that points at a project
    directory or a `CMakeLists.txt`: CMake configures the project and
    exports a `compile_commands.json` into a build directory, which is then
    read. Any scratch build directory is handed to the database so it
    outlives extraction.
*/
Expected<MrDocsCompilationDatabase>
generateCompilationDatabaseWithCMake(
    Config const& config,
    std::string const& projectPath)
{
    auto& settings = config;

    MRDOCS_TRY(BuildLocation build, resolveCMakeBuildLocation(settings));

    Expected<std::string> const compileCommandsPathExp =
        generateCompileCommandsFileWithCmake(
            projectPath, settings.cmake, build.buildPath, settings.configDir());
    if (!compileCommandsPathExp)
    {
        report::error(
            "Failed to generate compile_commands.json file: {}",
            compileCommandsPathExp.error());
        return Unexpected(compileCommandsPathExp.error());
    }

    MRDOCS_TRY(
        MrDocsCompilationDatabase database,
        readCompilationDatabase(config, *compileCommandsPathExp));

    // The compile commands point into the scratch build tree, so the
    // database owns it and keeps it alive through extraction.
    if (build.scratch)
    {
        database.keepAlive(std::move(*build.scratch));
    }
    return database;
}

} // anonymous namespace

Expected<MrDocsCompilationDatabase>
generateCompilationDatabase(
    Config const& config)
{
    // Find and validate the path that describes the compilation database.
    MRDOCS_TRY(
        std::string const dbPath,
        resolveCompilationDatabasePath(config));

    // Without a path, synthesize a database from the configuration values.
    if (dbPath.empty())
    {
        return synthesizeCompilationDatabase(config);
    }

    // resolveCompilationDatabasePath only ever returns an existing regular
    // file (a compile_commands.json or a CMakeLists.txt), never a directory.
    MRDOCS_ASSERT(files::isRegularFile(dbPath));

    // A compile_commands.json file is read directly
    auto filename = files::getFileName(dbPath);
    if (filename == "compile_commands.json")
    {
        return readCompilationDatabase(config, dbPath);
    }

    // A CMakeLists.txt is configured: run CMake to
    // generate a compile_commands.json, then read it
    MRDOCS_ASSERT(filename == "CMakeLists.txt");
    return generateCompilationDatabaseWithCMake(config, dbPath);
}

} // mrdocs

