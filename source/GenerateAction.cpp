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

#include "Options.hpp"
#include "ConfigImpl.hpp"
#include "CorpusImpl.hpp"
#include <mrdox/Generators.hpp>
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
        if(IgnoreMappingFailures.getValue())
            os << "ignore-failures: true\n";
    }

    // Load configuration file
    if(! ConfigPath.hasArgStr())
        return Error("the config path argument is missing");
    auto config = loadConfigFile(ConfigPath, extraYaml);
    if(! config)
        return config.getError();

    // Load the compilation database
    if(InputPaths.empty())
        return Error("the compilation database path argument is missing");
    if(InputPaths.size() > 1)
        return Error("got {} input paths where 1 was expected", InputPaths.size());
    std::string errorMessage;
    auto compilations = tooling::JSONCompilationDatabase::loadFromFile(
        InputPaths.front(), errorMessage, tooling::JSONCommandLineSyntax::AutoDetect);
    if(! compilations)
        return Error(std::move(errorMessage));

    // Create the ToolExecutor from the compilation database
    int ThreadCount = 0;
    auto ex = std::make_unique<tooling::AllTUsToolExecutor>(
        *compilations, ThreadCount);

    // Create the generator
    auto generator = generators.find(FormatType.getValue());
    if(! generator)
        return Error("the Generator \"{}\" was not found", FormatType.getValue());

    // Run the tool, this can take a while
    auto corpus = CorpusImpl::build(*ex, *config);
    if(! corpus)
        return Error("CorpusImpl::build returned \"{}\"", corpus.getError());

    // Run the generator.
    if(config.get()->verboseOutput)
        llvm::outs() << "Generating docs...\n";
    return generator->build(OutputPath.getValue(), **corpus);
}

} // mrdox
} // clang
