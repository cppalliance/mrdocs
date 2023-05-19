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
#include <mrdox/Generators.hpp>
#include <mrdox/Reporter.hpp>
#include <clang/Tooling/AllTUsExecution.h>
#include <clang/Tooling/JSONCompilationDatabase.h>
#include <cstdlib>

namespace clang {
namespace mrdox {

int
DoGenerateAction(Reporter& R)
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
    {
        llvm::errs() <<
            "Missing configuration file path argument.\n";
        return EXIT_FAILURE;
    }
    std::error_code ec;
    auto config = loadConfigFile(ConfigPath, extraYaml, ec);
    if(ec)
    {
        (void)R.error(ec, "load config file '", ConfigPath, "'");
        return EXIT_FAILURE;
    }

    // Load the compilation database
    if(InputPaths.empty())
    {
        llvm::errs() <<
            "Missing path to compilation database argument.\n";
        return EXIT_FAILURE;
    }
    if(InputPaths.size() > 1)
    {
        llvm::errs() <<
            "Expected one input path argument, got more than one.\n";
        return EXIT_FAILURE;
    }
    std::string errorMessage;
    auto compilations =
        tooling::JSONCompilationDatabase::loadFromFile(
            InputPaths.front(), errorMessage,
                tooling::JSONCommandLineSyntax::AutoDetect);
    if(! compilations)
    {
        llvm::errs() << errorMessage << '\n';
        return EXIT_FAILURE;
    }

    // Create the ToolExecutor from the compilation database
    int ThreadCount = 0;
    auto ex = std::make_unique<tooling::AllTUsToolExecutor>(
        *compilations, ThreadCount);

    // Create the generator
    auto generator = generators.find(FormatType.getValue());
    if(! generator)
    {
        R.print("Generator '", FormatType.getValue(), "' not found.");
        return EXIT_FAILURE;
    }

    // Run the tool, this can take a while
    auto corpus = Corpus::build(*ex, config, R);
    if(R.error(corpus, "build the documentation corpus"))
        return EXIT_FAILURE;

    // Run the generator.
    if(config->verboseOutput)
        llvm::outs() << "Generating docs...\n";
    auto err = generator->build(OutputPath.getValue(), **corpus, R);
    if(err)
    {
        R.print(err.message(), "generate '", OutputPath, "'");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

} // mrdox
} // clang
