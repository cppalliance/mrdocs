//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "CompilerInfo.hpp"
#include "ToolArgs.hpp"
#include <lib/Lib/CMakeExecution.hpp>
#include "lib/Lib/ConfigImpl.hpp"
#include "lib/Lib/CorpusImpl.hpp"
#include "lib/Lib/MrDocsCompilationDatabase.hpp"
#include "lib/Support/Path.hpp"
#include "llvm/Support/Program.h"
#include <mrdocs/Generators.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Path.hpp>
#include <mrdocs/Support/ThreadPool.hpp>
#include <clang/Tooling/JSONCompilationDatabase.h>

#include <cstdlib>

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


Expected<void>
DoGenerateAction(
    std::string const& configPath,
    ReferenceDirectories const& dirs,
    char const** argv)
{
    // --------------------------------------------------------------
    //
    // Load configuration
    //
    // --------------------------------------------------------------
    Config::Settings publicSettings;
    MRDOCS_TRY(Config::Settings::load_file(publicSettings, configPath, dirs));
    MRDOCS_TRY(toolArgs.apply(publicSettings, dirs, argv));
    MRDOCS_TRY(publicSettings.normalize(dirs));
    ThreadPool threadPool(publicSettings.concurrency);
    MRDOCS_TRY(
        std::shared_ptr<ConfigImpl const> config,
        ConfigImpl::load(publicSettings, dirs, threadPool));

    // --------------------------------------------------------------
    //
    // Load generator
    //
    // --------------------------------------------------------------
    auto& settings = config->settings();
    MRDOCS_TRY(
        Generator const& generator,
        getGenerators().find(to_string(settings.generator)),
        formatError(
            "the Generator \"{}\" was not found",
            to_string(config->settings().generator)));

    // --------------------------------------------------------------
    //
    // Find or generate the compile_commands.json
    //
    // --------------------------------------------------------------
    std::string compilationDatabasePath = settings.compilationDatabase;
    if (compilationDatabasePath.empty())
    {
        std::string c = files::appendPath(settings.sourceRoot, "CMakeLists.txt");
        if (files::exists(c))
        {
            compilationDatabasePath = c;
        }
    }
    MRDOCS_CHECK(
        compilationDatabasePath,
        "The compilation database path argument is missing");
    ScopedTempDirectory tempDir(config->settings().output, ".temp");
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
        jsonDatabase, (*config)->useSystemStdlib);
    auto compileCommandsDir = files::getParentDir(compileCommandsPath);
    MrDocsCompilationDatabase compilationDatabase(
        compileCommandsDir,
        jsonDatabase,
        config,
        defaultIncludePaths);

    // --------------------------------------------------------------
    //
    // Build corpus
    //
    // --------------------------------------------------------------
    MRDOCS_TRY(
        std::unique_ptr<Corpus> corpus,
        CorpusImpl::build(config, compilationDatabase));
    if (corpus->empty())
    {
        report::warn("Corpus is empty, not generating docs");
        return {};
    }

    // --------------------------------------------------------------
    //
    // Generate docs
    //
    // --------------------------------------------------------------
    // Normalize outputPath path
    MRDOCS_CHECK(settings.output, "The output path argument is missing");
    report::info("Generating docs\n");
    MRDOCS_TRY(generator.build(*corpus));

    // --------------------------------------------------------------
    //
    // Clean temp files
    //
    // --------------------------------------------------------------

    return {};
}

} // mrdocs
} // clang
