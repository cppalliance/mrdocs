//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "ToolCompilationDatabase.hpp"
#include "CompilerInfo.hpp"
#include "lib/MrDocsSettingsDB.hpp"
#include "lib/Support/CMakeExecution.hpp"
#include "lib/Support/Path.hpp"
#include <mrdocs/Support/Report.hpp>

namespace clang {
namespace mrdocs {

namespace {

/**
 * Conditionally generates a `compile_commands.json` file based on the provided project path.
 *
 * This function evaluates the project path to decide the appropriate action regarding the generation of a `compile_commands.json` file:
 * 1. If the project path is a `compile_commands.json` file, it returns the path as-is, with no database generation.
 * 2. If the project path is a directory, it generates the compilation database using the provided project path.
 * 3. If the project path is a `CMakeLists.txt` file, it generates the compilation database using the parent directory of the file.
 *
 * @param inputPath The path to the project, which can be a directory, a `compile_commands.json` file, or a `CMakeLists.txt` file.
 * @param cmakeArgs The arguments to pass to CMake when generating the compilation database.
 * @return An `Expected` object containing the path to the `compile_commands.json` file if the database is generated, or the provided path if it is already the `compile_commands.json` file.
 * Returns an `Unexpected` object in case of failure (e.g., file not found, CMake execution failure).
 */
Expected<std::string>
generateCompileCommandsFile(llvm::StringRef inputPath, llvm::StringRef cmakeArgs, llvm::StringRef buildDir)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    fs::file_status fileStatus;
    MRDOCS_CHECK(files::exists(inputPath), formatError("File does not exist: '{}'", inputPath.operator std::string_view()));
    MRDOCS_CHECK(!fs::status(inputPath, fileStatus), "Failed to get file status");

    // --------------------------------------------------------------
    // Input path is a project directory
    // --------------------------------------------------------------
    if (fs::is_directory(fileStatus))
    {
        return executeCmakeExportCompileCommands(inputPath, cmakeArgs, buildDir);
    }

    // --------------------------------------------------------------
    // Input path is a CMakeLists.txt
    // --------------------------------------------------------------
    auto const fileName = files::getFileName(inputPath);
    if (fileName == "CMakeLists.txt")
    {
        std::string cmakeSourceDir = files::getParentDir(inputPath);
        return executeCmakeExportCompileCommands(
            cmakeSourceDir, cmakeArgs, buildDir);
    }

    // --------------------------------------------------------------
    // Input path is a compile_commands.json
    // --------------------------------------------------------------
    if (fileName == "compile_commands.json")
    {
        return inputPath.str();
    }

    return Unexpected(Error("Input path is not a directory, a CMakeLists.txt file, or a compile_commands.json file"));
}

} // anonymous namespace

Expected<MrDocsCompilationDatabase>
generateCompilationDatabase(
    std::string_view tempDir,
    std::shared_ptr<ConfigImpl const> const& config,
    ThreadPool& threadPool)
{
    auto& settings = config->settings();
    std::string compilationDatabasePath = settings.compilationDatabase;

    // If no compilation database path is provided but we have a
    // cmake option, check if there is a CMakeLists.txt in the source root
    if (compilationDatabasePath.empty() &&
        !settings.cmake.empty())
    {
        std::string c = files::appendPath(settings.sourceRoot, "CMakeLists.txt");
        if (files::exists(c))
        {
            compilationDatabasePath = c;
        }
    }

    // If still no compilation database path is provided, we're going to
    // generate the compilation database from the configuration parameters
    if (compilationDatabasePath.empty())
    {
        MrDocsSettingsDB compilationDB{*config};
        auto const defaultIncludePaths = getCompilersDefaultIncludeDir(
            compilationDB);
        MrDocsCompilationDatabase compilationDatabase(
            settings.sourceRoot,
            compilationDB,
            config,
            defaultIncludePaths);
        return compilationDatabase;
    }

    MRDOCS_CHECK(
        compilationDatabasePath,
        "The compilation database path argument is missing");
    std::string buildPath = files::appendPath(tempDir, "build");
    Expected<std::string> const compileCommandsPathExp =
        generateCompileCommandsFile(
            compilationDatabasePath, settings.cmake, buildPath);
    if (!compileCommandsPathExp)
    {
        report::error(
            "Failed to generate compile_commands.json file: {}",
            compileCommandsPathExp.error());
        return Unexpected(compileCommandsPathExp.error());
    }

    // --------------------------------------------------------------
    //
    // Load the compilation database file
    //
    // --------------------------------------------------------------
    std::string compileCommandsPath = files::normalizePath(*compileCommandsPathExp);
    MRDOCS_TRY(compileCommandsPath, files::makeAbsolute(compileCommandsPath));
    std::string errorMessage;
    std::unique_ptr<clang::tooling::JSONCompilationDatabase>
        jsonDatabasePtr =
            tooling::JSONCompilationDatabase::loadFromFile(
                compileCommandsPath,
                errorMessage,
                tooling::JSONCommandLineSyntax::AutoDetect);
    if (!jsonDatabasePtr)
    {
        return Unexpected(formatError(
            "Failed to load compilation database: {}", errorMessage));
    }
    clang::tooling::JSONCompilationDatabase& jsonDatabase = *jsonDatabasePtr;

    // Custom compilation database that applies settings from the configuration
    auto const defaultIncludePaths = getCompilersDefaultIncludeDir(
        jsonDatabase);
    auto compileCommandsDir = files::getParentDir(compileCommandsPath);
    MrDocsCompilationDatabase compilationDatabase(
        compileCommandsDir,
        jsonDatabase,
        config,
        defaultIncludePaths);
    return compilationDatabase;
}

} // mrdocs
} // clang
