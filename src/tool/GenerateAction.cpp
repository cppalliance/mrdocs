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

#include "ToolArgs.hpp"
#include "lib/Lib/AbsoluteCompilationDatabase.hpp"
#include "lib/Lib/ConfigImpl.hpp"
#include "lib/Lib/CorpusImpl.hpp"
#include <mrdocs/Generators.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Path.hpp>
#include <clang/Tooling/JSONCompilationDatabase.h>
#include <cstdlib>

namespace clang {
namespace mrdocs {

Expected<void>
DoGenerateAction()
{
    // Get additional YAML settings from command line
    std::string extraYaml;
    {
        llvm::raw_string_ostream os(extraYaml);
        if (toolArgs.ignoreMappingFailures.getValue())
        {
            os << "ignore-failures: true\n";
        }
    }

    // Load YAML configuration file
    std::shared_ptr<ConfigImpl const> config;
    ThreadPool threadPool(toolArgs.concurrency);
    {
        MRDOCS_CHECK(toolArgs.configPath, "The config path argument is missing");
        MRDOCS_TRY(auto configFile, loadConfigFile(
            toolArgs.configPath,
            toolArgs.addonsDir,
            extraYaml,
            nullptr,
            threadPool));
        config = std::move(configFile);
    }

    // Get the generator
    MRDOCS_TRY(
        Generator const& generator,
        getGenerators().find(config->settings().generate),
        formatError(
            "the Generator \"{}\" was not found",
            config->settings().generate));

    // Load the compilation database
    MRDOCS_CHECK(toolArgs.inputPaths, "The compilation database path argument is missing");
    MRDOCS_CHECK(toolArgs.inputPaths.size() == 1,
        formatError(
            "got {} input paths where 1 was expected",
            toolArgs.inputPaths.size()));
    auto compilationsPath = files::normalizePath(toolArgs.inputPaths.front());
    std::string errorMessage;
    MRDOCS_TRY_MSG(
        auto& jsonCompilations,
        tooling::JSONCompilationDatabase::loadFromFile(
            compilationsPath,
            errorMessage,
            tooling::JSONCommandLineSyntax::AutoDetect),
        std::move(errorMessage));

    // Calculate the working directory
    MRDOCS_TRY(auto absPath, files::makeAbsolute(compilationsPath));
    auto workingDir = files::getParentDir(absPath);

    // Normalize outputPath
    MRDOCS_CHECK(toolArgs.outputPath, "The output path argument is missing");
    toolArgs.outputPath = files::normalizePath(
        files::makeAbsolute(toolArgs.outputPath,
            (*config)->workingDir));

    // Convert relative paths to absolute
    AbsoluteCompilationDatabase compilations(
        workingDir, jsonCompilations, config);

    // Run the tool: this can take a while
    MRDOCS_TRY(
        auto corpus,
        CorpusImpl::build(
            report::Level::info, config, compilations));

    // Run the generator
    report::info("Generating docs\n");
    MRDOCS_TRY(generator.build(toolArgs.outputPath.getValue(), *corpus));
    return {};
}

} // mrdocs
} // clang
