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
#include <lib/Support/ExecuteAndWaitWithLogging.hpp>
#include <mrdocs/Support/Error.hpp>
#include <llvm/Support/Program.h>


namespace mrdocs {

Optional<std::string>
getCompilerVerboseOutput(llvm::StringRef compilerPath) 
{
    if ( ! llvm::sys::fs::exists(compilerPath))
    {
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
    int const result = ExecuteAndWaitWithLogging(compilerPath, args, emptyEnv, redirects);
    if (result != 0) 
    {
        llvm::sys::fs::remove(outputPath);
        return std::nullopt;
    }

    auto bufferOrError = llvm::MemoryBuffer::getFile(outputPath);
    llvm::sys::fs::remove(outputPath);
    if (!bufferOrError)
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
getCompilersDefaultIncludeDir(clang::tooling::CompilationDatabase const& compDb, bool useSystemStdlib) 
{
    if (!useSystemStdlib)
    {
        return {};
    }
    std::unordered_map<std::string, std::vector<std::string>> res;
    auto const allCommands = compDb.getAllCompileCommands();

    for (auto const& cmd : allCommands)
    {
        if (!cmd.CommandLine.empty())
        {
            auto const& compilerPath = cmd.CommandLine[0];
            if (res.contains(compilerPath))
            {
                continue;
            }

            std::vector<std::string> includePaths;
            auto const compilerOutput = getCompilerVerboseOutput(compilerPath);
            if (!compilerOutput)
            {
                res.emplace(compilerPath, includePaths);
                continue;
            }
            includePaths = parseIncludePaths(*compilerOutput);
            res.emplace(compilerPath, std::move(includePaths));
        }
    }

    return res;
}

} // mrdocs

