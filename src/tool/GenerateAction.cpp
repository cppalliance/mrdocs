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
#include "lib/Lib/ConfigImpl.hpp"
#include "lib/Lib/CorpusImpl.hpp"
#include "lib/Lib/MrDocsCompilationDatabase.hpp"
#include "llvm/Support/Program.h"
#include <mrdocs/Generators.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Path.hpp>
#include <clang/Tooling/JSONCompilationDatabase.h>

#include <cstdlib>
#include <unordered_map>

namespace clang {
namespace mrdocs {

std::optional<std::string> 
getCompilerVerboseOutput(llvm::StringRef compilerPath) 
{
    if ( ! llvm::sys::fs::exists(compilerPath)) {
        return std::nullopt;
    }

    llvm::SmallString<128> outputPath;
    if (auto ec = llvm::sys::fs::createTemporaryFile("compiler-info", "txt", outputPath)) 
    {
        return std::nullopt;
    }

    std::optional<llvm::StringRef> const redirects[] = {llvm::StringRef(), llvm::StringRef(), outputPath.str()};
    std::vector<llvm::StringRef> const args = {compilerPath, "-v", "-E", "-x", "c++", "-"};
    llvm::ArrayRef<llvm::StringRef> emptyEnv;
    int const result = llvm::sys::ExecuteAndWait(compilerPath, args, emptyEnv, redirects);
    if (result != 0) 
    {
        llvm::sys::fs::remove(outputPath);
        return std::nullopt;
    }

    auto bufferOrError = llvm::MemoryBuffer::getFile(outputPath);
    llvm::sys::fs::remove(outputPath);
    if ( ! bufferOrError) 
    {
        return std::nullopt;
    }

    return bufferOrError.get()->getBuffer().str();
}

std::vector<std::string> 
parseIncludePaths(std::string const& compilerOutput) 
{
    std::vector<std::string> includePaths;
    std::istringstream stream(compilerOutput);
    std::string line;
    bool capture = false;

    while (std::getline(stream, line)) 
    {
        if (line.find("#include <...> search starts here:") != std::string::npos) 
        {
            capture = true;
            continue;
        }
        if (line.find("End of search list.") != std::string::npos) 
        {
            break;
        }
        if (capture) 
        {
            line.erase(0, line.find_first_not_of(" "));
            includePaths.push_back(line);
        }
    }

    return includePaths;
}

std::unordered_map<std::string, std::vector<std::string>> 
getCompilersDefaultIncludeDir(clang::tooling::CompilationDatabase const& compDb) 
{
    std::unordered_map<std::string, std::vector<std::string>> res;
    auto const allCommands = compDb.getAllCompileCommands();

    for (auto const& cmd : allCommands) {
        if ( ! cmd.CommandLine.empty()) {
            auto const& compilerPath = cmd.CommandLine[0];
            if (res.contains(compilerPath)) {
                continue;
            }

            // Skip MSVC compiler
            if (compilerPath.find("cl.exe") != std::string::npos) {
                continue;
            }   

            auto const compilerOutput = getCompilerVerboseOutput(compilerPath);
            if ( ! compilerOutput) {
                report::warn("Warning: could not get compiler info for \"{}\"", compilerPath);
                continue;
            }
            std::vector<std::string> includePaths = parseIncludePaths(*compilerOutput);
            res.emplace(compilerPath, std::move(includePaths));                      
        }
    }

    return res;
}

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
        auto& compileCommands,
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

    // Get the default include paths for each compiler
    auto const defaultIncludePaths = getCompilersDefaultIncludeDir(compileCommands);

    // Convert relative paths to absolute
    MrDocsCompilationDatabase compilations(
        workingDir, compileCommands, config, defaultIncludePaths);

    // --------------------------------------------------------------
    //
    // Build corpus
    //
    // --------------------------------------------------------------
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
