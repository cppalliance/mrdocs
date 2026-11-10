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
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <clang/Options/Options.h>
#include <llvm/Option/ArgList.h>
#include <llvm/Option/OptTable.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/Process.h>
#include <llvm/Support/Program.h>
#include <format>
#include <ranges>


namespace mrdocs {

Optional<std::string>
getCompilerVerboseOutput(
    llvm::StringRef compilerPath,
    report::Reporter& reporter,
    std::vector<std::string> const& sysrootArgs)
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
    std::vector<llvm::StringRef> args = {compilerPath};
    args.insert(args.end(), sysrootArgs.begin(), sysrootArgs.end());
    args.insert(args.end(), {"-v", "-E", "-x", "c++", "-"});
    llvm::ArrayRef<llvm::StringRef> emptyEnv;
    int const result = ExecuteAndWaitWithLogging(reporter, compilerPath, args, emptyEnv, redirects);
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
printFileName(
    llvm::StringRef compilerPath,
    llvm::StringRef name,
    report::Reporter& reporter)
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
    int const result = ExecuteAndWaitWithLogging(reporter, compilerPath, args, emptyEnv, redirects);
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
    std::vector<std::string>& includePaths,
    report::Reporter& reporter)
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
        auto answer = printFileName(compilerPath, name, reporter);
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

// True for a C++ standard library include directory in a probed search
// list: libstdc++ installs under .../c++/<version> and libc++ under
// .../c++/v1, so a "/c++/" path segment identifies both.
bool
isCppStdlibIncludeDir(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    return path.find("/c++/") != std::string::npos
        || path.ends_with("/c++");
}

// True for a Clang resource include directory (lib/clang/<version>/include)
// in a probed search list.
bool
isClangResourceIncludeDir(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    while (path.ends_with('/'))
    {
        path.pop_back();
    }
    return path.ends_with("/include")
        && path.find("/lib/clang/") != std::string::npos;
}

// Keep only the probed directories the configuration asks for. The probe
// returns the compiler's whole search list with the C++ standard library,
// the compiler's resource headers, and the C library mixed together, so
// each flag filters its own slice: with the bundled C++ stdlib, the host's
// C++ dirs (and the host compiler's resource dir, whose builtin headers
// belong to a different Clang) must not leak onto the path; with the
// bundled libc stubs, the host's C library dirs must not.
void
filterProbedIncludeDirs(
    bool const useSystemStdlib,
    bool const useSystemLibc,
    std::vector<std::string>& includePaths)
{
    if (useSystemStdlib && useSystemLibc)
    {
        return;
    }
    std::erase_if(includePaths, [&](std::string const& p) {
        if (isCppStdlibIncludeDir(p) || isClangResourceIncludeDir(p))
        {
            return !useSystemStdlib;
        }
        return !useSystemLibc;
    });
}

} // (anon)

namespace {

// Try to get include paths from a compiler found by name in PATH.
// Returns the include paths if successful, empty vector otherwise.
std::vector<std::string>
tryCompilerByName(llvm::StringRef name, report::Reporter& reporter)
{
    auto found = llvm::sys::findProgramByName(name);
    if (!found)
    {
        return {};
    }
    auto output = getCompilerVerboseOutput(*found, reporter);
    if (!output)
    {
        return {};
    }
    auto includePaths = parseIncludePaths(*output);
    removeGccBuiltinIncludeDirs(*found, *output, includePaths, reporter);
    return includePaths;
}

} // anonymous namespace

// `-isysroot` wins over `--sysroot` for headers, as in the driver. A compiler
// run outside its wrapper (the Xcode toolchain's clang rather than
// /usr/bin/c++) lists no SDK directories unless it is told the sysroot the
// compile command uses.
std::vector<std::string>
findSysrootArgs(
    std::vector<std::string> const& commandLine,
    llvm::StringRef const directory,
    llvm::StringRef const workingDir)
{
    if (commandLine.size() < 2)
    {
        return {};
    }
    auto const argv = std::views::transform(commandLine, &std::string::c_str);
    std::vector<char const*> const argPtrs(argv.begin() + 1, argv.end());
    unsigned missingIndex = 0;
    unsigned missingCount = 0;
    llvm::opt::InputArgList const args = clang::getDriverOptTable().ParseArgs(
        argPtrs,
        missingIndex,
        missingCount,
        llvm::opt::Visibility(clang::options::ClangOption));
    llvm::opt::Arg const* arg = args.getLastArg(clang::options::OPT_isysroot);
    if (!arg)
    {
        arg = args.getLastArg(clang::options::OPT__sysroot_EQ);
    }
    if (!arg)
    {
        return {};
    }
    std::string const dir = files::makeAbsolute(
        arg->getValue(), files::makeAbsolute(directory, workingDir));
    if (arg->getOption().matches(clang::options::OPT_isysroot))
    {
        return {"-isysroot", dir};
    }
    return {std::format("--sysroot={}", dir)};
}

Optional<std::string>
resolveCompilerPath(
    llvm::StringRef const compiler,
    llvm::StringRef const directory,
    llvm::StringRef const workingDir)
{
    if (compiler.empty())
    {
        return std::nullopt;
    }
    if (compiler.find_first_of("/\\") == llvm::StringRef::npos)
    {
        // A bare name is run through the PATH, as the driver would.
        if (auto found = llvm::sys::findProgramByName(compiler))
        {
            return std::move(*found);
        }
        return std::nullopt;
    }
    std::string const path = files::makeAbsolute(
        compiler, files::makeAbsolute(directory, workingDir));
    if (llvm::sys::fs::exists(path))
    {
        return path;
    }
    return std::nullopt;
}

std::string
implicitIncludeKey(
    llvm::StringRef const compilerPath,
    std::vector<std::string> const& sysrootArgs)
{
    std::string key(compilerPath);
    for (auto const& arg : sysrootArgs)
    {
        key += '\n';
        key += arg;
    }
    return key;
}

std::vector<std::string>
fallbackCompilerCandidates(
    llvm::StringRef const cxx,
    llvm::StringRef const cc)
{
    std::vector<std::string> res;
    for (llvm::StringRef const env: {cxx, cc})
    {
        if (!env.empty())
        {
            res.emplace_back(env);
        }
    }
    for (std::string_view const name: {"g++", "clang++", "c++", "gcc", "clang"})
    {
        res.emplace_back(name);
    }
    return res;
}

std::unordered_map<std::string, std::vector<std::string>>
getCompilersDefaultIncludeDir(
    clang::tooling::CompilationDatabase const& compDb,
    llvm::StringRef const workingDir,
    bool const useSystemStdlib,
    bool const useSystemLibc,
    report::Reporter& reporter)
{
    if (!useSystemStdlib && !useSystemLibc)
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
            auto const sysrootArgs =
                findSysrootArgs(cmd.CommandLine, cmd.Directory, workingDir);
            auto const key = implicitIncludeKey(compilerPath, sysrootArgs);
            if (res.contains(key))
            {
                continue;
            }

            // Try the compiler specified in the compilation database. The
            // key keeps the name as written in the command, which is what
            // the lookup of the command's include dirs uses.
            auto const resolvedPath =
                resolveCompilerPath(compilerPath, cmd.Directory, workingDir);
            auto compilerOutput = resolvedPath
                ? getCompilerVerboseOutput(*resolvedPath, reporter, sysrootArgs)
                : std::nullopt;
            if (compilerOutput)
            {
                auto includePaths = parseIncludePaths(*compilerOutput);
                removeGccBuiltinIncludeDirs(
                    *resolvedPath, *compilerOutput, includePaths, reporter);
                filterProbedIncludeDirs(
                    useSystemStdlib, useSystemLibc, includePaths);
                res.emplace(key, std::move(includePaths));
                continue;
            }

            // The compiler from the database wasn't found.
            // Probe the compiler the user chose (CXX, then CC) and then
            // common ones to discover system include paths.
            auto const fallbackCompilers = fallbackCompilerCandidates(
                llvm::sys::Process::GetEnv("CXX").value_or(""),
                llvm::sys::Process::GetEnv("CC").value_or(""));
            std::vector<std::string> includePaths;
            for (auto const& fallback : fallbackCompilers)
            {
                includePaths = tryCompilerByName(fallback, reporter);
                if (!includePaths.empty())
                {
                    break;
                }
            }
            filterProbedIncludeDirs(
                useSystemStdlib, useSystemLibc, includePaths);
            res.emplace(key, std::move(includePaths));
        }
    }

    return res;
}

} // mrdocs
