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
#include "ConfigImpl.hpp"
#include "CorpusImpl.hpp"
#include "AST/AbsoluteCompilationDatabase.hpp"
#include <mrdox/Generators.hpp>
#include <mrdox/Support/Report.hpp>
#include <mrdox/Support/Path.hpp>
#include <clang/Tooling/AllTUsExecution.h>
#include <clang/Tooling/JSONCompilationDatabase.h>
#include <cstdlib>

namespace clang {
namespace mrdox {

Error
DoGenerateAction()
{
    auto& generators = getGenerators();

    // Calculate additional YAML settings from command line options.
    std::string extraYaml;
    {
        llvm::raw_string_ostream os(extraYaml);
        if(toolArgs.ignoreMappingFailures.getValue())
            os << "ignore-failures: true\n";
    }

    // Load configuration file
    if(! toolArgs.configPath.hasArgStr())
        return Error("the config path argument is missing");
    auto config = loadConfigFile(toolArgs.configPath, extraYaml);
    if(! config)
        return config.getError();

    // Load the compilation database
    if(toolArgs.inputPaths.empty())
        return Error("the compilation database path argument is missing");
    if(toolArgs.inputPaths.size() > 1)
        return Error("got {} input paths where 1 was expected", toolArgs.inputPaths.size());
    auto compilationsPath = files::normalizePath(toolArgs.inputPaths.front());
    std::string errorMessage;
    auto jsonCompilations = tooling::JSONCompilationDatabase::loadFromFile(
        compilationsPath, errorMessage, tooling::JSONCommandLineSyntax::AutoDetect);
    if(! jsonCompilations)
        return Error(std::move(errorMessage));

    // Calculate the working directory
    auto absPath = files::makeAbsolute(compilationsPath);
    if(! absPath)
        return absPath.getError();
    auto workingDir = files::getParentDir(*absPath);

    // Convert relative paths to absolute
    AbsoluteCompilationDatabase compilations(
        workingDir, *jsonCompilations, *config);

    // Create the ToolExecutor from the compilation database
    int ThreadCount = 0;
    auto ex = std::make_unique<tooling::AllTUsToolExecutor>(compilations, ThreadCount);

    // Create the generator
    auto generator = generators.find(toolArgs.formatType.getValue());
    if(! generator)
        return Error("the Generator \"{}\" was not found", toolArgs.formatType.getValue());

    // Run the tool, this can take a while
    auto corpus = CorpusImpl::build(*ex, *config);
    if(! corpus)
        return Error("CorpusImpl::build returned \"{}\"", corpus.getError());

    // Run the generator.
    if(config.get()->verboseOutput)
        reportInfo("Generating docs...\n");
    return generator->build(toolArgs.outputPath.getValue(), **corpus);
}

} // mrdox
} // clang
