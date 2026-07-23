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

#include "MrDocsCompilationDatabase.hpp"
#include <lib/AST/ClangHelpers.hpp>
#include <lib/ConfigImpl.hpp>
#include <lib/Support/Debug.hpp>
#include <lib/Support/ExecuteAndWaitWithLogging.hpp>
#include <lib/Support/Path.hpp>
#include <mrdocs/Support/Report.hpp>
#include <clang/Basic/LangStandard.h>
#include <clang/Driver/Driver.h>
#include <clang/Driver/Types.h>
#include <clang/Options/Options.h>
#include <llvm/Option/ArgList.h>
#include <llvm/Option/OptTable.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Program.h>
#include <llvm/TargetParser/Host.h>
#include <format>
#include <ranges>


namespace mrdocs {

static
bool
isCXXSrcFile(
    std::string_view filename)
{
    llvm::StringRef ext = llvm::sys::path::extension(filename).drop_front();
    clang::driver::types::ID extensionId = clang::driver::types::lookupTypeForExtension(ext);
    return clang::driver::types::isCXX(extensionId);
}

static
bool
isCXXHeaderFile(
    std::string_view filename)
{
    llvm::StringRef ext = llvm::sys::path::extension(filename).drop_front();
    return ext == "hpp" || ext == "hh" || ext == "hxx" || ext == "h++";
}

static
bool
isCSrcFile(
    std::string_view filename)
{
    llvm::StringRef ext = llvm::sys::path::extension(filename).drop_front();
    return ext == "c";
}

static
bool
isCHeaderFile(
    std::string_view filename)
{
    llvm::StringRef ext = llvm::sys::path::extension(filename).drop_front();
    return ext == "h";
}

template<typename... Opts>
static
bool
optionMatchesAny(
    llvm::opt::Option const& opt,
    Opts&&... opts)
{
    return (opt.matches(opts) || ...);
}

static
bool
isValidMrDocsOption(
    llvm::StringRef workingDir,
    std::unique_ptr<llvm::opt::Arg> const &arg)
{
    // Unknown option
    if (!arg)
    {
        return false;
    }

    // Parsed argument
    const llvm::opt::Option opt =
        arg->getOption().getUnaliasedOption();

    if (optionMatchesAny(opt,
             // unknown options
             clang::options::OPT_UNKNOWN,

             // sanitizers
             clang::options::OPT_fsanitize_EQ,
             clang::options::OPT_fno_sanitize_EQ,
             clang::options::OPT_fsanitize_recover_EQ,
             clang::options::OPT_fno_sanitize_recover_EQ,
             clang::options::OPT_fsanitize_trap_EQ,
             clang::options::OPT_fno_sanitize_trap_EQ,
             clang::options::OPT_fsanitize_address_use_after_scope,
             clang::options::OPT_fexperimental_sanitize_metadata_ignorelist_EQ,
             clang::options::OPT_fexperimental_sanitize_metadata_EQ_atomics,
             clang::options::OPT_fexperimental_sanitize_metadata_EQ_covered,
             clang::options::OPT_fexperimental_sanitize_metadata_EQ,
             clang::options::OPT_fgpu_sanitize,
             clang::options::OPT_fno_experimental_sanitize_metadata_EQ,
             clang::options::OPT_fno_gpu_sanitize,
             clang::options::OPT_fno_sanitize_address_globals_dead_stripping,
             clang::options::OPT_fno_sanitize_address_outline_instrumentation,
             clang::options::OPT_fno_sanitize_address_poison_custom_array_cookie,
             clang::options::OPT_fno_sanitize_address_use_after_scope,
             clang::options::OPT_fno_sanitize_address_use_odr_indicator,
             clang::options::OPT__SLASH_fno_sanitize_address_vcasan_lib,
             clang::options::OPT_fno_sanitize_cfi_canonical_jump_tables,
             clang::options::OPT_fno_sanitize_cfi_cross_dso,
             clang::options::OPT_fno_sanitize_coverage,
             clang::options::OPT_fno_sanitize_hwaddress_experimental_aliasing,
             clang::options::OPT_fno_sanitize_ignorelist,
             clang::options::OPT_fno_sanitize_link_cxx_runtime,
             clang::options::OPT_fno_sanitize_link_runtime,
             clang::options::OPT_fno_sanitize_memory_param_retval,
             clang::options::OPT_fno_sanitize_memory_track_origins,
             clang::options::OPT_fno_sanitize_memory_use_after_dtor,
             clang::options::OPT_fno_sanitize_minimal_runtime,
             clang::options::OPT_fno_sanitize_recover_EQ,
             clang::options::OPT_fno_sanitize_recover,
             clang::options::OPT_fno_sanitize_stable_abi,
             clang::options::OPT_fno_sanitize_stats,
             clang::options::OPT_fno_sanitize_thread_atomics,
             clang::options::OPT_fno_sanitize_thread_func_entry_exit,
             clang::options::OPT_fno_sanitize_thread_memory_access,
             clang::options::OPT_fno_sanitize_trap_EQ,
             clang::options::OPT_fno_sanitize_trap,
             clang::options::OPT_fno_sanitize_undefined_trap_on_error,
             clang::options::OPT_fno_sanitize_EQ,
             clang::options::OPT_sanitize_address_destructor_EQ,
             clang::options::OPT_fsanitize_address_field_padding,
             clang::options::OPT_fsanitize_address_globals_dead_stripping,
             clang::options::OPT_fsanitize_address_outline_instrumentation,
             clang::options::OPT_fsanitize_address_poison_custom_array_cookie,
             clang::options::OPT_sanitize_address_use_after_return_EQ,
             clang::options::OPT__SLASH_fsanitize_address_use_after_return,
             clang::options::OPT_fsanitize_address_use_after_scope,
             clang::options::OPT_fsanitize_address_use_odr_indicator,
             clang::options::OPT_fsanitize_cfi_canonical_jump_tables,
             clang::options::OPT_fsanitize_cfi_cross_dso,
             clang::options::OPT_fsanitize_cfi_icall_normalize_integers,
             clang::options::OPT_fsanitize_cfi_icall_generalize_pointers,
             clang::options::OPT_fsanitize_coverage_8bit_counters,
             clang::options::OPT_fsanitize_coverage_allowlist,
             clang::options::OPT_fsanitize_coverage_control_flow,
             clang::options::OPT_fsanitize_coverage_ignorelist,
             clang::options::OPT_fsanitize_coverage_indirect_calls,
             clang::options::OPT_fsanitize_coverage_inline_8bit_counters,
             clang::options::OPT_fsanitize_coverage_inline_bool_flag,
             clang::options::OPT_fsanitize_coverage_no_prune,
             clang::options::OPT_fsanitize_coverage_pc_table,
             clang::options::OPT_fsanitize_coverage_stack_depth,
             clang::options::OPT_fsanitize_coverage_trace_bb,
             clang::options::OPT_fsanitize_coverage_trace_cmp,
             clang::options::OPT_fsanitize_coverage_trace_div,
             clang::options::OPT_fsanitize_coverage_trace_gep,
             clang::options::OPT_fsanitize_coverage_trace_loads,
             clang::options::OPT_fsanitize_coverage_trace_pc_guard,
             clang::options::OPT_fsanitize_coverage_trace_pc,
             clang::options::OPT_fsanitize_coverage_trace_stores,
             clang::options::OPT_fsanitize_coverage_type,
             clang::options::OPT_fsanitize_coverage,
             clang::options::OPT_fsanitize_hwaddress_abi_EQ,
             clang::options::OPT_fsanitize_hwaddress_experimental_aliasing,
             clang::options::OPT_fsanitize_ignorelist_EQ,
             clang::options::OPT_fsanitize_link_cxx_runtime,
             clang::options::OPT_fsanitize_link_runtime,
             clang::options::OPT_fsanitize_memory_param_retval,
             clang::options::OPT_fsanitize_memory_track_origins_EQ,
             clang::options::OPT_fsanitize_memory_track_origins,
             clang::options::OPT_fsanitize_memory_use_after_dtor,
             clang::options::OPT_fsanitize_memtag_mode_EQ,
             clang::options::OPT_fsanitize_minimal_runtime,
             clang::options::OPT_fsanitize_recover_EQ,
             clang::options::OPT_fsanitize_recover,
             clang::options::OPT_fsanitize_stable_abi,
             clang::options::OPT_fsanitize_stats,
             clang::options::OPT_fsanitize_system_ignorelist_EQ,
             clang::options::OPT_fsanitize_thread_atomics,
             clang::options::OPT_fsanitize_thread_func_entry_exit,
             clang::options::OPT_fsanitize_thread_memory_access,
             clang::options::OPT_fsanitize_trap_EQ,
             clang::options::OPT_fsanitize_trap,
             clang::options::OPT_fsanitize_undefined_strip_path_components_EQ,
             clang::options::OPT_fsanitize_undefined_trap_on_error,
             clang::options::OPT__SLASH_fsanitize_EQ_address,
             clang::options::OPT_fsanitize_EQ,
             clang::options::OPT_shared_libsan,
             clang::options::OPT_static_libsan,
             clang::options::OPT_static_libsan,

             // diagnostic options
             clang::options::OPT_Diag_Group,
             clang::options::OPT_W_value_Group,
             clang::options::OPT__SLASH_wd,

             // language conformance options
             clang::options::OPT_pedantic_Group,
             clang::options::OPT__SLASH_permissive,
             clang::options::OPT__SLASH_permissive_,

             // ignored options
             clang::options::OPT_cl_ignored_Group,
             clang::options::OPT_cl_ignored_Group,
             clang::options::OPT_clang_ignored_f_Group,
             clang::options::OPT_clang_ignored_gcc_optimization_f_Group,
             clang::options::OPT_clang_ignored_legacy_options_Group,
             clang::options::OPT_clang_ignored_m_Group,
             clang::options::OPT_flang_ignored_w_Group
#if 0
            // input file options
            clang::options::OPT_INPUT,

            // output file options
            clang::options::OPT_o,
            clang::options::OPT__SLASH_o,
            clang::options::OPT__SLASH_Fo,
            clang::options::OPT__SLASH_Fe,
            clang::options::OPT__SLASH_Fd,
            clang::options::OPT__SLASH_FA,
            clang::options::OPT__SLASH_Fa,
            clang::options::OPT__SLASH_Fi,
            clang::options::OPT__SLASH_FR,
            clang::options::OPT__SLASH_Fr,
            clang::options::OPT__SLASH_Fm,
            clang::options::OPT__SLASH_Fx,
#endif
            // clang::options::OPT__SLASH_TP
            // clang::options::OPT__SLASH_Tp
            // clang::options::OPT__SLASH_TC
            // clang::options::OPT__SLASH_Tc
    ))
    {
        return false;
    }

    // Unknown module files
    // Some versions of CMake include unexisting module files in the compile
    // commands file with the Clang toolchain.
    if (opt.getName() == "<input>")
    {
        auto& argv = *arg;
        std::string_view path = argv.getValue();
        bool const isCMakePath = path.starts_with("@CMakeFiles\\") || path.starts_with("@CMakeFiles/");
        bool const isModulePath = path.ends_with(".obj.modmap");
        if (isCMakePath && isModulePath)
        {
            constexpr std::size_t nChars = sizeof("@CMakeFiles/") - 1;
            std::string_view relPath = path.substr(nChars);
            auto moduleFile = files::appendPath(workingDir, "CMakeFiles", relPath);
            if (!files::exists(moduleFile))
            {
                return false;
            }
        }
    }
    return true;
}

static std::vector<std::string>
adjustCommandLine(
    llvm::StringRef const workingDir,
    std::vector<std::string> const& cmdline,
    bool is_clang_cl,
    std::shared_ptr<Config const> const& config,
    std::unordered_map<std::string, std::vector<std::string>> const&
        implicitIncludeDirectories,
    std::string_view filename)
{
    if (cmdline.empty())
    {
        return cmdline;
    }

    // ------------------------------------------------------
    // Copy the compiler path
    // ------------------------------------------------------
    std::string const& progName = cmdline.front();
    std::vector new_cmdline = {progName};

    // ------------------------------------------------------
    // Convert to InputArgList
    // ------------------------------------------------------
    // InputArgList is the input format for llvm functions
    auto cmdLineCStrsView = std::views::transform(cmdline, &std::string::c_str);
    std::vector const cmdLineCStrs(cmdLineCStrsView.begin(), cmdLineCStrsView.end());
    llvm::opt::InputArgList const args(
        cmdLineCStrs.data(),
        cmdLineCStrs.data() + cmdLineCStrs.size());

    char const* systemIncludeFlag = is_clang_cl ? "-external:I" : "-isystem";
    // FIXME: No CL equivalent, and not really needed for the microsoft system
    // headers, but other users could depend on this.
    char const* afterIncludeFlag = is_clang_cl ? "-external:I" : "-idirafter";
    char const* latestCxxStandardFlag = is_clang_cl ? "-std:c++latest" : "-std=c++26";
    char const* latestCStandardFlag = is_clang_cl ? "-std:clatest" : "-std=c23";
    char const* noStdlibFlag = is_clang_cl ? "-X" : "-nostdinc++";
    char const* noSystemLibFlag = is_clang_cl ? "-X" : "-nostdlibinc";

    // ------------------------------------------------------
    // Supress all warnings
    // ------------------------------------------------------
    // Add flags to ignore all warnings. Any options that
    // affect warnings will be discarded later.
    new_cmdline.emplace_back("-w");
    new_cmdline.emplace_back("-fsyntax-only");

    // ------------------------------------------------------
    // Language standard
    // ------------------------------------------------------
    // If cmdline contains `-x c` or `-x c++`, then the
    // language is explicitly set.
    bool isExplicitCppCompileCommand = false;
    bool isExplicitCCompileCommand = false;
    constexpr auto is_x_option = [](std::string_view const opt) {
        return opt == "-x" || opt == "--language";
    };
    if (auto const it = std::ranges::find_if(cmdline, is_x_option);
            it != cmdline.end())
    {
        if (auto const next = std::next(it);
            next != cmdline.end())
        {
            isExplicitCppCompileCommand = *next == "c++";
            isExplicitCCompileCommand = *next == "c";
        }
    }
    bool const isImplicitCSourceFile = isCSrcFile(filename);
    bool const isCCompileCommand =
        isExplicitCCompileCommand || (!isExplicitCppCompileCommand && isImplicitCSourceFile);

    constexpr auto is_std_option = [](std::string_view const opt) {
        return opt.starts_with("-std=") || opt.starts_with("--std=")
               || // clang options
               opt.starts_with("-std:")
               || opt.starts_with("/std:"); // clang-cl options
    };
    // If the language standard wasn't specified, change the default to the
    // latest one available.
    if (std::ranges::none_of(cmdline, is_std_option))
    {
        if (!isCCompileCommand)
        {
            new_cmdline.emplace_back(latestCxxStandardFlag);
        }
        else
        {
            new_cmdline.emplace_back(latestCStandardFlag);
        }
    }

    // ------------------------------------------------------
    // Add additional defines
    // ------------------------------------------------------
    // These are additional defines specified in the config file
    for(auto const& def : (*config)->defines)
    {
        new_cmdline.emplace_back(std::format("-D{}", def));
    }
    new_cmdline.emplace_back("-D__MRDOCS__");

    // Additional compiler arguments from the config file
    for (auto const& arg : (*config)->extraCompilerArgs)
    {
        new_cmdline.emplace_back(arg);
    }

    if ((*config)->useSystemStdlib || (*config)->useSystemLibc)
    {
        // ------------------------------------------------------
        // Add implicit include paths
        // ------------------------------------------------------
        // Implicit include paths are those which are automatically
        // added by the compiler. These will not be defined in the
        // compile command, so we add them here so that clang
        // can also find these headers.
        if (auto const it = implicitIncludeDirectories.find(progName);
            it != implicitIncludeDirectories.end()) {
            for (auto const& inc : it->second)
            {
                new_cmdline.emplace_back(systemIncludeFlag);
                new_cmdline.emplace_back(inc);
            }
        }
    }

    if (!(*config)->useSystemStdlib)
    {
        // ------------------------------------------------------
        // Add standard library and system includes
        // ------------------------------------------------------
        // Regardless of the implicit include directories of the
        // compiler used in the compilation database, we disable
        // implicit include paths and add the standard library
        // and system includes manually. That gives MrDocs
        // access to libc++ in a portable way.
        new_cmdline.emplace_back(noStdlibFlag);
        for (auto const& inc : (*config)->stdlibIncludes)
        {
            new_cmdline.emplace_back(systemIncludeFlag);
            new_cmdline.emplace_back(inc);
        }
    }

    if (!(*config)->useSystemLibc)
    {
        new_cmdline.emplace_back(noSystemLibFlag);
        for (auto const& inc : (*config)->libcIncludes)
        {
            new_cmdline.emplace_back(afterIncludeFlag);
            new_cmdline.emplace_back(inc);
        }
    }

    // ------------------------------------------------------
    // Add user directories to include search path
    // ------------------------------------------------------
    for (auto const& inc : (*config)->systemIncludes)
    {
        new_cmdline.emplace_back(systemIncludeFlag);
        new_cmdline.emplace_back(inc);
    }
    for (auto const& inc : (*config)->includes)
    {
      new_cmdline.emplace_back(std::format("-I{}", inc));
    }

    // ------------------------------------------------------
    // Adjust each argument in the command line
    // ------------------------------------------------------
    // Iterate over each argument in the command line and
    // add it to the new command line if it is a valid
    // Clang option. This will discard any options that
    // affect warnings, are ignored, or turn warnings into
    // errors.
    llvm::opt::OptTable const& opts_table = clang::getDriverOptTable();
    llvm::opt::Visibility visibility(is_clang_cl ?
        clang::options::CLOption : clang::options::ClangOption);
    unsigned idx = 1;
    while (idx < cmdline.size())
    {
        // Parse one argument as a Clang option
        // ParseOneArg updates Index to the next argument to be parsed.
        unsigned const idx0 = idx;
        std::unique_ptr<llvm::opt::Arg> arg =
            opts_table.ParseOneArg(args, idx, visibility);
        if (!isValidMrDocsOption(workingDir, arg))
        {
            continue;
        }
        new_cmdline.insert(
            new_cmdline.end(),
            cmdline.begin() + idx0,
            cmdline.begin() + idx);
    }

    return new_cmdline;
}

static
std::string
makeAbsoluteAndNative(
    llvm::StringRef workingDir,
    llvm::StringRef path)
{
    SmallPathString temp;
    if (llvm::sys::path::is_absolute(path))
    {
        llvm::sys::path::native(path, temp);
    }
    else
    {
        temp = path;
        llvm::sys::path::make_absolute(workingDir, temp);
        llvm::sys::path::remove_dots(temp, true);
    }
    return static_cast<std::string>(temp);
}

MrDocsCompilationDatabase::
MrDocsCompilationDatabase(
    llvm::StringRef const workingDir,
    CompilationDatabase const& inner,
    std::shared_ptr<Config const> const& config,
    std::unordered_map<std::string, std::vector<std::string>> const& implicitIncludeDirectories)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;
    using clang::tooling::CompileCommand;

    std::vector<CompileCommand> allCommands = inner.getAllCompileCommands();
    if (allCommands.empty())
    {
        return;
    }

    isClangCL_ = mrdocs::isClangCL(allCommands.front());
    AllCommands_.reserve(allCommands.size());
    SmallPathString temp;
    for (CompileCommand const& cmd0 : allCommands)
    {
        CompileCommand cmd;
        cmd.CommandLine = cmd0.CommandLine;
        cmd.Heuristic = cmd0.Heuristic;
        cmd.Output = cmd0.Output;
        cmd.CommandLine = adjustCommandLine(
            workingDir,
            cmd0.CommandLine,
            isClangCL_,
            config,
            implicitIncludeDirectories,
            cmd0.Filename);
        cmd.Directory = makeAbsoluteAndNative(workingDir, cmd0.Directory);
        cmd.Filename = makeAbsoluteAndNative(workingDir, cmd0.Filename);
        if (
            isCXXSrcFile(cmd.Filename) ||
            isCSrcFile(cmd.Filename) ||
            isCXXHeaderFile(cmd.Filename) ||
            isCHeaderFile(cmd.Filename))
        {
            const bool emplaced = IndexByFile_.try_emplace(cmd.Filename, AllCommands_.size()).second;
            if (emplaced)
            {
                AllCommands_.emplace_back(std::move(cmd));
            }
        }
        else
        {
          report::info(std::format("Skipping non-C++ file: {}", cmd.Filename));
        }
    }
}

void
MrDocsCompilationDatabase::
keepAlive(ScopedTempDirectory&& dir)
{
    scratchDirs_.push_back(std::move(dir));
}

std::vector<clang::tooling::CompileCommand>
MrDocsCompilationDatabase::
getCompileCommands(
    llvm::StringRef FilePath) const
{
    SmallPathString nativeFilePath;
    llvm::sys::path::native(FilePath, nativeFilePath);

    auto const it = IndexByFile_.find(nativeFilePath);
    if (it == IndexByFile_.end())
        return {};
    std::vector<clang::tooling::CompileCommand> Commands;
    Commands.push_back(AllCommands_[it->getValue()]);
    return Commands;
}

std::vector<std::string>
MrDocsCompilationDatabase::
getAllFiles() const
{
    std::vector<std::string> allFiles;
    allFiles.reserve(AllCommands_.size());
    for(auto const& cmd : AllCommands_)
        allFiles.push_back(cmd.Filename);
    return allFiles;
}

std::vector<clang::tooling::CompileCommand>
MrDocsCompilationDatabase::
getAllCompileCommands() const
{
    return AllCommands_;
}

} // mrdocs

