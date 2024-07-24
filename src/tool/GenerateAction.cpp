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

#include "Addons.hpp"
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
        return executeCmakeExportCompileCommands(files::getParentDir(inputPath), cmakeArgs, buildDir);
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
DoGenerateAction(std::string_view execPath, char const** argv)
{
    // --------------------------------------------------------------
    //
    // Load configuration
    //
    // --------------------------------------------------------------
    Config::Settings publicSettings;
    MRDOCS_TRY(toolArgs.apply(publicSettings, execPath, argv));
    ThreadPool threadPool(toolArgs.concurrency);
    MRDOCS_TRY(
        std::shared_ptr<ConfigImpl const> config,
        loadConfigFile(publicSettings, nullptr, threadPool, execPath));

    // --------------------------------------------------------------
    //
    // Load generator
    //
    // --------------------------------------------------------------
    auto const& settings = config->settings();
    MRDOCS_TRY(
        Generator const& generator,
        getGenerators().find(settings.generate),
        formatError(
            "the Generator \"{}\" was not found",
            config->settings().generate));

    // --------------------------------------------------------------
    //
    // Find or generate the compile_commands.json
    //
    // --------------------------------------------------------------
    MRDOCS_CHECK(
        settings.compilationDatabase,
        "The compilation database path argument is missing");
    ScopedTempDirectory tempDir("mrdocs");
    Expected<std::string> const compileCommandsPathExp =
        generateCompileCommandsFile(
            settings.compilationDatabase,
            settings.cmake,
            files::appendPath(tempDir.path(), "build"));
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
    MRDOCS_TRY_MSG(
        auto& compileCommands,
        tooling::JSONCompilationDatabase::loadFromFile(
            compileCommandsPath,
            errorMessage,
            tooling::JSONCommandLineSyntax::AutoDetect),
        std::move(errorMessage));

    // Custom compilation database that converts relative paths to absolute
    auto const defaultIncludePaths = getCompilersDefaultIncludeDir(compileCommands);
    auto compileCommandsDir = files::getParentDir(compileCommandsPath);
    MRDOCS_ASSERT(files::isDirsy(compileCommandsDir));
    MrDocsCompilationDatabase compilationDatabase(
        compileCommandsDir,
        compileCommands, config,
        defaultIncludePaths);

    // --------------------------------------------------------------
    //
    // Build corpus
    //
    // --------------------------------------------------------------
    MRDOCS_TRY(
        std::unique_ptr<Corpus> corpus,
        CorpusImpl::build(
            report::Level::info, config, compilationDatabase));
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
    std::string absOutput = files::normalizePath(
        files::makeAbsolute(
            settings.output,
            (*config)->configDir));
    report::info("Generating docs\n");
    MRDOCS_TRY(generator.build(absOutput, *corpus));
    return {};
}

} // mrdocs
} // clang
