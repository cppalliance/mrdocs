//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "CompilerInfo.hpp"
#include "Support/ExecuteAndWaitWithLogging.hpp"
#include <mrdocs/Support/Error/Error.hpp>
#include <llvm/Support/Program.h>
#include <format>


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

namespace {

// Run the compiler with `-print-file-name=<name>` and return its answer, or
// nothing when the compiler cannot be run. The compiler prints the argument
// back verbatim when it has no such file.
Optional<std::string>
printFileName(llvm::StringRef compilerPath, llvm::StringRef name)
{
    llvm::SmallString<128> outputPath;
    if (llvm::sys::fs::createTemporaryFile("compiler-file-name", "txt", outputPath))
    {
        return std::nullopt;
    }
    std::string const arg = std::format("-print-file-name={}", std::string_view(name));
    std::optional<llvm::StringRef> const redirects[] =
        {llvm::StringRef(), outputPath.str(), llvm::StringRef()};
    std::vector<llvm::StringRef> const args = {compilerPath, arg};
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
    std::string answer = bufferOrError.get()->getBuffer().trim().str();
    return answer;
}

// Remove GCC's private builtin include directories from a probed search list.
//
// GCC's lib/gcc/<triple>/<version>/include (and include-fixed) hold GCC's own
// intrinsics headers (xmmintrin.h and friends), which redefine functions
// MrDocs' Clang treats as builtins, so extraction fails with "definition of
// builtin function" on any code that touches SIMD. Clang supplies those
// headers from its resource directory, and a real clang driver never puts
// GCC's private directory on the search path.
//
// The directories are identified by asking the same compiler for them with
// `-print-file-name=include`, so only the exact directories GCC names are
// removed; user paths that merely look similar are never touched. The whole
// step only applies when the probed compiler is GCC (its verbose output
// carries a "gcc version" line); a Clang's resource directory is kept, since
// on macOS the SDK's include_next chains depend on it.
void
removeGccBuiltinIncludeDirs(
    llvm::StringRef compilerPath,
    std::string const& verboseOutput,
    std::vector<std::string>& includePaths)
{
    if (verboseOutput.find("gcc version") == std::string::npos)
    {
        return;
    }
    // Compare resolved paths: the two GCC outputs can spell the same
    // directory differently (one with bin/../lib segments, one canonical).
    auto resolved = [](llvm::StringRef p) -> std::string {
        llvm::SmallString<256> out;
        if (llvm::sys::fs::real_path(p, out))
        {
            return p.str();
        }
        return std::string(out);
    };
    for (llvm::StringRef const name: {"include", "include-fixed"})
    {
        auto answer = printFileName(compilerPath, name);
        // The compiler echoes the bare name back when it has no such
        // directory; only an absolute answer identifies a real one.
        if (!answer || *answer == name)
        {
            continue;
        }
        std::string const target = resolved(*answer);
        std::erase_if(includePaths, [&](std::string const& p) {
            return resolved(p) == target;
        });
    }
}

} // (anon)

namespace {

// Try to get include paths from a compiler found by name in PATH.
// Returns the include paths if successful, empty vector otherwise.
std::vector<std::string>
tryCompilerByName(llvm::StringRef name)
{
    auto found = llvm::sys::findProgramByName(name);
    if (!found)
    {
        return {};
    }
    auto output = getCompilerVerboseOutput(*found);
    if (!output)
    {
        return {};
    }
    auto includePaths = parseIncludePaths(*output);
    removeGccBuiltinIncludeDirs(*found, *output, includePaths);
    return includePaths;
}

} // anonymous namespace

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

            // Try the compiler specified in the compilation database
            auto compilerOutput = getCompilerVerboseOutput(compilerPath);
            if (compilerOutput)
            {
                auto includePaths = parseIncludePaths(*compilerOutput);
                removeGccBuiltinIncludeDirs(
                    compilerPath, *compilerOutput, includePaths);
                res.emplace(compilerPath, std::move(includePaths));
                continue;
            }

            // The compiler from the database wasn't found.
            // Try common fallback compilers to discover system
            // include paths.
            static constexpr std::string_view fallbackCompilers[] = {
                "g++", "clang++", "c++", "gcc", "clang"
            };
            std::vector<std::string> includePaths;
            for (auto const& fallback : fallbackCompilers)
            {
                includePaths = tryCompilerByName(fallback);
                if (!includePaths.empty())
                {
                    break;
                }
            }
            res.emplace(compilerPath, std::move(includePaths));
        }
    }

    return res;
}

} // mrdocs
