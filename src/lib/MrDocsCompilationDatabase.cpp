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
#include "lib/ConfigImpl.hpp"
#include "lib/Support/Debug.hpp"
#include "lib/Support/ExecuteAndWaitWithLogging.hpp"
#include "lib/Support/Path.hpp"
#include <clang/Basic/LangStandard.h>
#include <clang/Driver/Driver.h>
#include <clang/Driver/Options.h>
#include <clang/Driver/Types.h>
#include <format>
#include <llvm/Option/ArgList.h>
#include <llvm/Option/OptTable.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Program.h>
#include <llvm/TargetParser/Host.h>
#include <mrdocs/Support/Report.hpp>
#include <ranges>

namespace clang {
namespace mrdocs {

static
bool
isCXXSrcFile(
    std::string_view filename)
{
    StringRef ext = llvm::sys::path::extension(filename).drop_front();
    driver::types::ID extensionId = driver::types::lookupTypeForExtension(ext);
    return driver::types::isCXX(extensionId);
}

static
bool
isCXXHeaderFile(
    std::string_view filename)
{
    StringRef ext = llvm::sys::path::extension(filename).drop_front();
    return ext == "hpp" || ext == "hh" || ext == "hxx" || ext == "h++";
}

static
bool
isCSrcFile(
    std::string_view filename)
{
    StringRef ext = llvm::sys::path::extension(filename).drop_front();
    return ext == "c";
}

static
bool
isCHeaderFile(
    std::string_view filename)
{
    StringRef ext = llvm::sys::path::extension(filename).drop_front();
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
             driver::options::OPT_UNKNOWN,

             // sanitizers
             driver::options::OPT_fsanitize_EQ,
             driver::options::OPT_fno_sanitize_EQ,
             driver::options::OPT_fsanitize_recover_EQ,
             driver::options::OPT_fno_sanitize_recover_EQ,
             driver::options::OPT_fsanitize_trap_EQ,
             driver::options::OPT_fno_sanitize_trap_EQ,
             driver::options::OPT_fsanitize_address_use_after_scope,
             driver::options::OPT_fexperimental_sanitize_metadata_ignorelist_EQ,
             driver::options::OPT_fexperimental_sanitize_metadata_EQ_atomics,
             driver::options::OPT_fexperimental_sanitize_metadata_EQ_covered,
             driver::options::OPT_fexperimental_sanitize_metadata_EQ,
             driver::options::OPT_fgpu_sanitize,
             driver::options::OPT_fno_experimental_sanitize_metadata_EQ,
             driver::options::OPT_fno_gpu_sanitize,
             driver::options::OPT_fno_sanitize_address_globals_dead_stripping,
             driver::options::OPT_fno_sanitize_address_outline_instrumentation,
             driver::options::OPT_fno_sanitize_address_poison_custom_array_cookie,
             driver::options::OPT_fno_sanitize_address_use_after_scope,
             driver::options::OPT_fno_sanitize_address_use_odr_indicator,
             driver::options::OPT__SLASH_fno_sanitize_address_vcasan_lib,
             driver::options::OPT_fno_sanitize_cfi_canonical_jump_tables,
             driver::options::OPT_fno_sanitize_cfi_cross_dso,
             driver::options::OPT_fno_sanitize_coverage,
             driver::options::OPT_fno_sanitize_hwaddress_experimental_aliasing,
             driver::options::OPT_fno_sanitize_ignorelist,
             driver::options::OPT_fno_sanitize_link_cxx_runtime,
             driver::options::OPT_fno_sanitize_link_runtime,
             driver::options::OPT_fno_sanitize_memory_param_retval,
             driver::options::OPT_fno_sanitize_memory_track_origins,
             driver::options::OPT_fno_sanitize_memory_use_after_dtor,
             driver::options::OPT_fno_sanitize_minimal_runtime,
             driver::options::OPT_fno_sanitize_recover_EQ,
             driver::options::OPT_fno_sanitize_recover,
             driver::options::OPT_fno_sanitize_stable_abi,
             driver::options::OPT_fno_sanitize_stats,
             driver::options::OPT_fno_sanitize_thread_atomics,
             driver::options::OPT_fno_sanitize_thread_func_entry_exit,
             driver::options::OPT_fno_sanitize_thread_memory_access,
             driver::options::OPT_fno_sanitize_trap_EQ,
             driver::options::OPT_fno_sanitize_trap,
             driver::options::OPT_fno_sanitize_undefined_trap_on_error,
             driver::options::OPT_fno_sanitize_EQ,
             driver::options::OPT_sanitize_address_destructor_EQ,
             driver::options::OPT_fsanitize_address_field_padding,
             driver::options::OPT_fsanitize_address_globals_dead_stripping,
             driver::options::OPT_fsanitize_address_outline_instrumentation,
             driver::options::OPT_fsanitize_address_poison_custom_array_cookie,
             driver::options::OPT_sanitize_address_use_after_return_EQ,
             driver::options::OPT__SLASH_fsanitize_address_use_after_return,
             driver::options::OPT_fsanitize_address_use_after_scope,
             driver::options::OPT_fsanitize_address_use_odr_indicator,
             driver::options::OPT_fsanitize_cfi_canonical_jump_tables,
             driver::options::OPT_fsanitize_cfi_cross_dso,
             driver::options::OPT_fsanitize_cfi_icall_normalize_integers,
             driver::options::OPT_fsanitize_cfi_icall_generalize_pointers,
             driver::options::OPT_fsanitize_coverage_8bit_counters,
             driver::options::OPT_fsanitize_coverage_allowlist,
             driver::options::OPT_fsanitize_coverage_control_flow,
             driver::options::OPT_fsanitize_coverage_ignorelist,
             driver::options::OPT_fsanitize_coverage_indirect_calls,
             driver::options::OPT_fsanitize_coverage_inline_8bit_counters,
             driver::options::OPT_fsanitize_coverage_inline_bool_flag,
             driver::options::OPT_fsanitize_coverage_no_prune,
             driver::options::OPT_fsanitize_coverage_pc_table,
             driver::options::OPT_fsanitize_coverage_stack_depth,
             driver::options::OPT_fsanitize_coverage_trace_bb,
             driver::options::OPT_fsanitize_coverage_trace_cmp,
             driver::options::OPT_fsanitize_coverage_trace_div,
             driver::options::OPT_fsanitize_coverage_trace_gep,
             driver::options::OPT_fsanitize_coverage_trace_loads,
             driver::options::OPT_fsanitize_coverage_trace_pc_guard,
             driver::options::OPT_fsanitize_coverage_trace_pc,
             driver::options::OPT_fsanitize_coverage_trace_stores,
             driver::options::OPT_fsanitize_coverage_type,
             driver::options::OPT_fsanitize_coverage,
             driver::options::OPT_fsanitize_hwaddress_abi_EQ,
             driver::options::OPT_fsanitize_hwaddress_experimental_aliasing,
             driver::options::OPT_fsanitize_ignorelist_EQ,
             driver::options::OPT_fsanitize_link_cxx_runtime,
             driver::options::OPT_fsanitize_link_runtime,
             driver::options::OPT_fsanitize_memory_param_retval,
             driver::options::OPT_fsanitize_memory_track_origins_EQ,
             driver::options::OPT_fsanitize_memory_track_origins,
             driver::options::OPT_fsanitize_memory_use_after_dtor,
             driver::options::OPT_fsanitize_memtag_mode_EQ,
             driver::options::OPT_fsanitize_minimal_runtime,
             driver::options::OPT_fsanitize_recover_EQ,
             driver::options::OPT_fsanitize_recover,
             driver::options::OPT_fsanitize_stable_abi,
             driver::options::OPT_fsanitize_stats,
             driver::options::OPT_fsanitize_system_ignorelist_EQ,
             driver::options::OPT_fsanitize_thread_atomics,
             driver::options::OPT_fsanitize_thread_func_entry_exit,
             driver::options::OPT_fsanitize_thread_memory_access,
             driver::options::OPT_fsanitize_trap_EQ,
             driver::options::OPT_fsanitize_trap,
             driver::options::OPT_fsanitize_undefined_strip_path_components_EQ,
             driver::options::OPT_fsanitize_undefined_trap_on_error,
             driver::options::OPT__SLASH_fsanitize_EQ_address,
             driver::options::OPT_fsanitize_EQ,
             driver::options::OPT_shared_libsan,
             driver::options::OPT_static_libsan,
             driver::options::OPT_static_libsan,

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
             driver::options::OPT_flang_ignored_w_Group,

            // input file options
            //driver::options::OPT_INPUT,

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
            driver::options::OPT__SLASH_TP,
            driver::options::OPT__SLASH_Tp,
            driver::options::OPT__SLASH_TC,
            driver::options::OPT__SLASH_Tc
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

static
std::vector<std::string>
adjustCommandLine(
    StringRef const workingDir,
    std::vector<std::string> const& cmdline,
    std::shared_ptr<Config const> const& config,
    std::unordered_map<std::string, std::vector<std::string>> const& implicitIncludeDirectories,
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
    std::vector new_cmdline = {std::string{"clang"}};

    // ------------------------------------------------------
    // Convert to InputArgList
    // ------------------------------------------------------
    // InputArgList is the input format for llvm functions
    auto cmdLineCStrsView = std::views::transform(cmdline, &std::string::c_str);
    std::vector const cmdLineCStrs(cmdLineCStrsView.begin(), cmdLineCStrsView.end());
    llvm::opt::InputArgList const args(
        cmdLineCStrs.data(),
        cmdLineCStrs.data() + cmdLineCStrs.size());

    // ------------------------------------------------------
    // Get driver mode
    // ------------------------------------------------------
    // The driver mode distinguishes between clang/gcc and msvc
    // command line option formats. The value is deduced from
    // the `-drive-mode` option or from `progName`.
    // Common values are "gcc", "g++", "cpp", "cl" and "flang".
    StringRef const driver_mode = driver::getDriverMode(progName, cmdLineCStrs);
    // Identify if we should use "msvc/clang-cl" or "clang/gcc" format
    // for options.
    bool const is_clang_cl = driver::IsClangCL(driver_mode);

    // ------------------------------------------------------
    // Supress all warnings
    // ------------------------------------------------------
    // Add flags to ignore all warnings. Any options that
    // affect warnings will be discarded later.
    new_cmdline.emplace_back("-w");
    new_cmdline.emplace_back("-fsyntax-only");

    // ------------------------------------------------------
    // Target architecture
    // ------------------------------------------------------
    constexpr auto is_target_option = [](std::string_view opt) {
        return opt == "-target" || opt == "--target";
    };
    if (std::ranges::find_if(cmdline, is_target_option) == cmdline.end())
    {
        auto getCommandCompilerTarget = [&]() -> std::string {
            ScopedTempFile const outputPath("compiler-triple", "txt");
            if (!outputPath) {
                return {};
            }
            std::vector<llvm::StringRef> args = {
                progName, "--print-target-triple"
            };
            std::optional<llvm::StringRef> const redirects[] = {
                llvm::StringRef(),
                outputPath.path(),
                llvm::StringRef()
            };
            int const result = ExecuteAndWaitWithLogging(
                progName, args, std::nullopt, redirects);
            if (result != 0)
            {
                return {};
            }

            auto const bufferOrError = llvm::MemoryBuffer::getFile(
                outputPath.path());
            if (!bufferOrError) {
                return {};
            }
            return bufferOrError.get()->getBuffer().trim().str();
        };

        [&]() {
            std::string target = llvm::sys::getDefaultTargetTriple();

            if (target.empty())
            {
                target = llvm::sys::getProcessTriple();
            }

            if (target.empty())
            {
                target = getCommandCompilerTarget();
            }

#if defined(__APPLE__)
            if (target.empty())
            {
                target = "arm64-apple-darwin24.0.0";
            }
#else
            if (target.empty())
            {
                return;
            }
#endif

            new_cmdline.emplace_back("-target");
            new_cmdline.emplace_back(target);
        }();
    }

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
        return opt.starts_with("-std=") || opt.starts_with("--std=") || opt.starts_with("/std:");
    };
    if (std::ranges::find_if(cmdline, is_std_option) == cmdline.end())
    {
        if (!isCCompileCommand)
        {
            new_cmdline.emplace_back("-std=c++23");
        }
        else
        {
            new_cmdline.emplace_back("-std=c23");
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
              new_cmdline.emplace_back(std::format("-isystem{}", inc));
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
        new_cmdline.emplace_back("-nostdinc++");
        new_cmdline.emplace_back("-nostdlib++");
        for (auto const& inc : (*config)->stdlibIncludes)
        {
          new_cmdline.emplace_back(std::format("-isystem{}", inc));
        }
    }

    if (!(*config)->useSystemLibc)
    {
        new_cmdline.emplace_back("-nostdinc");
        for (auto const& inc : (*config)->libcIncludes)
        {
          new_cmdline.emplace_back(std::format("-isystem{}", inc));
        }
    }

    // ------------------------------------------------------
    // Add user directories to include search path
    // ------------------------------------------------------
    for (auto const& inc : (*config)->systemIncludes)
    {
      new_cmdline.emplace_back(std::format("-isystem{}", inc));
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
    llvm::opt::OptTable const& opts_table = clang::driver::getDriverOptTable();
    llvm::opt::Visibility visibility(is_clang_cl ?
        driver::options::CLOption : driver::options::ClangOption);
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

        // Append the translated arguments to the new command line
        llvm::opt::ArgStringList output;
        arg->render(args, output);

        for (auto const& v : output) {
            new_cmdline.push_back(v);
        }
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
        llvm::sys::fs::make_absolute(workingDir, temp);
        llvm::sys::path::remove_dots(temp, true);
    }
    return static_cast<std::string>(temp);
}

MrDocsCompilationDatabase::
MrDocsCompilationDatabase(
    StringRef const workingDir,
    CompilationDatabase const& inner,
    std::shared_ptr<Config const> const& config,
    std::unordered_map<std::string, std::vector<std::string>> const& implicitIncludeDirectories)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;
    using tooling::CompileCommand;

    std::vector<CompileCommand> allCommands = inner.getAllCompileCommands();
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

std::vector<tooling::CompileCommand>
MrDocsCompilationDatabase::
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
MrDocsCompilationDatabase::
getAllFiles() const
{
    std::vector<std::string> allFiles;
    allFiles.reserve(AllCommands_.size());
    for(auto const& cmd : AllCommands_)
        allFiles.push_back(cmd.Filename);
    return allFiles;
}

std::vector<tooling::CompileCommand>
MrDocsCompilationDatabase::
getAllCompileCommands() const
{
    return AllCommands_;
}

} // mrdocs
} // clang
