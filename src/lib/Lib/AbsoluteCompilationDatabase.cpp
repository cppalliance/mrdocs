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

#include "lib/Support/Debug.hpp"
#include "lib/Support/Path.hpp"
#include "lib/Lib/ConfigImpl.hpp"
#include "lib/Lib/AbsoluteCompilationDatabase.hpp"
#include <fmt/format.h>
#include <clang/Basic/LangStandard.h>
#include <clang/Driver/Driver.h>
#include <clang/Driver/Options.h>
#include <clang/Driver/Types.h>
#include <llvm/Option/ArgList.h>
#include <llvm/Option/OptSpecifier.h>
#include <llvm/Option/OptTable.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

namespace clang {
namespace mrdocs {

static
bool
isCXXSrcFile(
    std::string_view filename)
{
    return driver::types::isCXX(
        driver::types::lookupTypeForExtension(
            llvm::sys::path::extension(filename).drop_front()));
}

template<typename... Opts>
static
bool
optionMatchesAny(
    const llvm::opt::Option& opt,
    Opts&&... opts)
{
    return (opt.matches(opts) || ...);
}

static
std::vector<std::string>
adjustCommandLine(
    const std::vector<std::string>& cmdline,
    const std::vector<std::string>& additional_defines,
    std::unordered_map<std::string, std::vector<std::string>> const& includePathsByCompiler)
{
    std::vector<std::string> new_cmdline;
    std::vector<std::string> discarded_cmdline;
    llvm::opt::InputArgList args;
    StringRef driver_mode;

    std::vector<std::string> systemIncludePaths;

    if( ! cmdline.empty())
    {
        if (auto it = includePathsByCompiler.find(cmdline[0]); it != includePathsByCompiler.end()) {
            systemIncludePaths = it->second;
        }

        std::vector<const char*> raw_cmdline;
        raw_cmdline.reserve(cmdline.size());
        for(const auto& s : cmdline)
            raw_cmdline.push_back(s.c_str());
        args = llvm::opt::InputArgList(raw_cmdline.data(),
            raw_cmdline.data() + raw_cmdline.size());
        driver_mode = driver::getDriverMode(
            raw_cmdline.front(), raw_cmdline);
        new_cmdline.push_back(cmdline.front());
    }
    const llvm::opt::OptTable& opts_table =
        clang::driver::getDriverOptTable();

    bool is_clang_cl = ! cmdline.empty() &&
        driver::IsClangCL(driver_mode);
    llvm::opt::Visibility visibility(is_clang_cl ?
        driver::options::CLOption :
        driver::options::ClangOption);
    // suppress all warnings
    new_cmdline.emplace_back(
        is_clang_cl ? "/w" : "-w");
    new_cmdline.emplace_back("-fsyntax-only");

    for(const auto& def : additional_defines)
        new_cmdline.emplace_back(fmt::format("-D{}", def));

    for (auto const& inc : systemIncludePaths)
        new_cmdline.emplace_back(fmt::format("-I{}", inc));

    for(unsigned idx = 1; idx < cmdline.size();)
    {
        const unsigned old_idx = idx;
        std::unique_ptr<llvm::opt::Arg> arg =
            opts_table.ParseOneArg(args, idx, visibility);

        if(! arg)
        {
            discarded_cmdline.insert(
                discarded_cmdline.end(),
                cmdline.begin() + old_idx,
                cmdline.begin() + idx);
            continue;
        }

        const llvm::opt::Option opt =
            arg->getOption().getUnaliasedOption();

        // discard the option if it affects warnings,
        // is ignored, or turns warnings into errors
        if(optionMatchesAny(opt,
            // unknown options
            driver::options::OPT_UNKNOWN,
            // diagnostic options
            driver::options::OPT_Diag_Group,
            driver::options::OPT_W_value_Group,
            driver::options::OPT__SLASH_wd,
            // language conformance options
            driver::options::OPT_pedantic_Group,
            driver::options::OPT__SLASH_permissive,
            driver::options::OPT__SLASH_permissive_,

            // ignored options
            driver::options::OPT_cl_ignored_Group,
            driver::options::OPT_cl_ignored_Group,
            driver::options::OPT_clang_ignored_f_Group,
            driver::options::OPT_clang_ignored_gcc_optimization_f_Group,
            driver::options::OPT_clang_ignored_legacy_options_Group,
            driver::options::OPT_clang_ignored_m_Group,
            driver::options::OPT_flang_ignored_w_Group
#if 0
            // input file options
            driver::options::OPT_INPUT,
            // output file options
            driver::options::OPT_o,
            driver::options::OPT__SLASH_o,
            driver::options::OPT__SLASH_Fo,
            driver::options::OPT__SLASH_Fe,
            driver::options::OPT__SLASH_Fd,
            driver::options::OPT__SLASH_FA,
            driver::options::OPT__SLASH_Fa,
            driver::options::OPT__SLASH_Fi,
            driver::options::OPT__SLASH_FR,
            driver::options::OPT__SLASH_Fr,
            driver::options::OPT__SLASH_Fm,
            driver::options::OPT__SLASH_Fx,
#endif
            // driver::options::OPT__SLASH_TP
            // driver::options::OPT__SLASH_Tp
            // driver::options::OPT__SLASH_TC
            // driver::options::OPT__SLASH_Tc
            ))
        {
            discarded_cmdline.insert(
                discarded_cmdline.end(),
                cmdline.begin() + old_idx,
                cmdline.begin() + idx);
            continue;
        }

        new_cmdline.insert(
            new_cmdline.end(),
            cmdline.begin() + old_idx,
            cmdline.begin() + idx);
    }

    return new_cmdline;
}

AbsoluteCompilationDatabase::
AbsoluteCompilationDatabase(
    llvm::StringRef workingDir,
    CompilationDatabase const& inner,
    std::shared_ptr<const Config> config,
    std::unordered_map<std::string, std::vector<std::string>> const& includePathsByCompiler)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;
    auto config_impl = std::dynamic_pointer_cast<
        const ConfigImpl>(config);

    auto allCommands = inner.getAllCompileCommands();
    AllCommands_.reserve(allCommands.size());
    SmallPathString temp;
    for(auto const& cmd0 : allCommands)
    {
        tooling::CompileCommand cmd;

        cmd.CommandLine = cmd0.CommandLine;
        cmd.Heuristic = cmd0.Heuristic;
        cmd.Output = cmd0.Output;
        cmd.CommandLine = adjustCommandLine(
            cmd0.CommandLine,
            (*config_impl)->defines,
            includePathsByCompiler);

        if(path::is_absolute(cmd0.Directory))
        {
            path::native(cmd0.Directory, temp);
            cmd.Directory = static_cast<std::string>(temp);
        }
        else
        {
            temp = cmd0.Directory;
            fs::make_absolute(workingDir, temp);
            path::remove_dots(temp, true);
            cmd.Directory = static_cast<std::string>(temp);
        }

        if(path::is_absolute(cmd0.Filename))
        {
            path::native(cmd0.Filename, temp);
            cmd.Filename = static_cast<std::string>(temp);
        }
        else
        {
            temp = cmd0.Filename;
            fs::make_absolute(workingDir, temp);
            path::remove_dots(temp, true);
            cmd.Filename = static_cast<std::string>(temp);
        }

        // non-C++ input file; skip
        if(! isCXXSrcFile(cmd.Filename))
            continue;

        std::size_t i = AllCommands_.size();
        auto result = IndexByFile_.try_emplace(cmd.Filename, i);
        if(result.second)
            AllCommands_.emplace_back(std::move(cmd));
    }
}

std::vector<tooling::CompileCommand>
AbsoluteCompilationDatabase::
getCompileCommands(
    llvm::StringRef FilePath) const
{
    SmallPathString nativeFilePath;
    llvm::sys::path::native(FilePath, nativeFilePath);

    auto const it = IndexByFile_.find(nativeFilePath);
    if (it == IndexByFile_.end())
        return {};
    std::vector<tooling::CompileCommand> Commands;
    Commands.push_back(AllCommands_[it->getValue()]);
    return Commands;
}

std::vector<std::string>
AbsoluteCompilationDatabase::
getAllFiles() const
{
    std::vector<std::string> allFiles;
    allFiles.reserve(AllCommands_.size());
    for(auto const& cmd : AllCommands_)
        allFiles.push_back(cmd.Filename);
    return allFiles;
}

std::vector<tooling::CompileCommand>
AbsoluteCompilationDatabase::
getAllCompileCommands() const
{
    return AllCommands_;
}

} // mrdocs
} // clang
