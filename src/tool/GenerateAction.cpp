//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "ToolArgs.hpp"
#include "lib/Lib/AbsoluteCompilationDatabase.hpp"
#include "lib/Lib/ConfigImpl.hpp"
#include "lib/Lib/CorpusImpl.hpp"
#include <mrdox/Generators.hpp>
#include <mrdox/Support/Error.hpp>
#include <mrdox/Support/Path.hpp>
#include <clang/Tooling/JSONCompilationDatabase.h>
#include <cstdlib>

namespace clang {
namespace mrdox {

Error
DoGenerateAction()
{
    auto& generators = getGenerators();

    ThreadPool threadPool(toolArgs.concurrency);

    // Calculate additional YAML settings from command line options.
    std::string extraYaml;
    {
        llvm::raw_string_ostream os(extraYaml);
        if(toolArgs.ignoreMappingFailures.getValue())
            os << "ignore-failures: true\n";
    }

    std::shared_ptr<ConfigImpl const> config;
    {
        // Load configuration file
        if(toolArgs.configPath.empty())
            return formatError("the config path argument is missing");
        auto configFile = loadConfigFile(
            toolArgs.configPath,
            toolArgs.addonsDir,
            extraYaml,
            nullptr,
            threadPool);
        if(! configFile)
            return configFile.error();
        config = std::move(configFile.value());
    }


    // Create the generator
    auto generator = generators.find(toolArgs.formatType.getValue());
    if(! generator)
        return formatError("the Generator \"{}\" was not found",
            toolArgs.formatType.getValue());

    // Load the compilation database
    if(toolArgs.inputPaths.empty())
        return formatError("the compilation database path argument is missing");
    if(toolArgs.inputPaths.size() > 1)
        return formatError("got {} input paths where 1 was expected", toolArgs.inputPaths.size());
    auto compilationsPath = files::normalizePath(toolArgs.inputPaths.front());
    std::string errorMessage;
    auto jsonCompilations = tooling::JSONCompilationDatabase::loadFromFile(
        compilationsPath, errorMessage, tooling::JSONCommandLineSyntax::AutoDetect);
    if(! jsonCompilations)
        return Error(std::move(errorMessage));

    // Calculate the working directory
    auto absPath = files::makeAbsolute(compilationsPath);
    if(! absPath)
        return absPath.error();
    auto workingDir = files::getParentDir(*absPath);

    // normalize outputPath
    if( toolArgs.outputPath.empty())
        return formatError("output path is empty");
    toolArgs.outputPath = files::normalizePath(
        files::makeAbsolute(toolArgs.outputPath,
            (*config)->workingDir));

    // Convert relative paths to absolute
    AbsoluteCompilationDatabase compilations(
        workingDir, *jsonCompilations, config);

    // Run the tool, this can take a while
    auto corpus = CorpusImpl::build(
        report::Level::info, config, compilations);
    if(! corpus)
        return formatError("CorpusImpl::build returned \"{}\"", corpus.error());

    // Run the generator.
    report::info("Generating docs\n");
    return generator->build(toolArgs.outputPath.getValue(), **corpus);
}

} // mrdox
} // clang
