//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Config.hpp>
#include <mrdocs/ConfigSchema.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Extensions/ExtensionRegistry.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Plugin.hpp>
#include <mrdocs/Support/Chrono.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <mrdocs/Support/Report.hpp>
#include <mrdocs/Transform.hpp>
#include <mrdocs/Version.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Host.h>
#include <chrono>
#include <cstdlib>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

extern int main(int argc, char const** argv);

namespace mrdocs {

//------------------------------------------------
//
// Command-line frontend
//
//------------------------------------------------

// The tool adds no options of its own: every option is a config option, so the
// command line is parsed straight from argv against the reflected config schema
// and the values are applied by Config::load_file. There is no generated
// options table and no llvm::cl registration.
static char const* const usageText = "Generate C++ reference documentation";

// The result of scanning argv: the framework flags, the config-file override,
// the positional inputs (config / compile-commands files), and any tokens that
// name no option.
struct CommandLine
{
    bool showHelp = false;
    bool showVersion = false;
    std::string configOption;
    std::vector<std::string> inputs;
    std::vector<std::string> unknownOptions;
};

// Pad `text` to `width` columns (a left-justified name column for --help).
static void
printPadded(llvm::raw_ostream& os, std::string_view text, std::size_t width)
{
    os << text;
    if (text.size() < width)
    {
        os << std::string(width - text.size(), ' ');
    }
}

// Print the option list grouped by category, straight from the schema's
// command-line metadata. This is why the tool needs no generated options
// table: the schema already carries every option's name, category, and brief.
static void
printHelp(llvm::raw_ostream& os)
{
    os << "USAGE: mrdocs [options] [<config-file>] [<compile-commands>]\n\n";
    os << usageText << "\n\n";
    os << "EXAMPLES:\n";
    os << "    mrdocs\n";
    os << "    mrdocs docs/mrdocs.yml\n";
    os << "    mrdocs docs/mrdocs.yml ../build/compile_commands.json\n\n";
    os << "OPTIONS:\n";
    std::string_view currentCategory;
    for (auto const& info : ConfigSchema::commandLineOptionInfos())
    {
        if (info.category != currentCategory)
        {
            currentCategory = info.category;
            os << "\n  " << currentCategory << ":\n";
        }
        std::string flag = "--";
        flag += info.name;
        if (info.takesValue)
        {
            flag += "=<value>";
        }
        os << "    ";
        printPadded(os, flag, 42);
        os << info.brief << "\n";
    }
    os << "\n  General:\n";
    os << "    ";
    printPadded(os, "--help", 42);
    os << "Print this help and exit.\n";
    os << "    ";
    printPadded(os, "--version", 42);
    os << "Print version information and exit.\n";
}

static void
printVersion(llvm::raw_ostream& os, std::string const& execPath)
{
    os << project_name << " version " << project_version_with_build << "\n";
    os << "Built with LLVM " << LLVM_VERSION_STRING << "\n";
    os << "Build SHA: " << project_version_build << "\n";
    os << "Target: " << llvm::sys::getDefaultTargetTriple() << "\n";
    os << "InstalledDir: " << files::getParentDir(execPath) << "\n";
}

// Scan argv into a CommandLine. The schema's command-line metadata tells us
// which options take a value, so `--opt value` is not mistaken for a positional
// input. Option values themselves are applied later by Config::load_file from
// the same argv; here we only need the framework flags, the config path, and
// the positionals.
static CommandLine
parseCommandLine(int argc, char const** argv)
{
    CommandLine cl;
    auto const infos = ConfigSchema::commandLineOptionInfos();
    auto findInfo = [&](std::string_view key)
        -> ConfigSchema::CommandLineOptionInfo const*
    {
        for (auto const& info : infos)
        {
            if (info.name == key)
            {
                return &info;
            }
        }
        return nullptr;
    };
    for (int i = 1; i < argc; ++i)
    {
        std::string_view const arg(argv[i]);
        if (arg == "--help" || arg == "-h")
        {
            cl.showHelp = true;
            continue;
        }
        if (arg == "--version")
        {
            cl.showVersion = true;
            continue;
        }
        if (!arg.starts_with("--"))
        {
            cl.inputs.emplace_back(arg);
            continue;
        }
        std::string_view const body = arg.substr(2);
        auto const eq = body.find('=');
        bool const hasValue = eq != std::string_view::npos;
        std::string_view const key =
            hasValue ? body.substr(0, eq) : body;
        // A dotted key (`--generator-options.x=y`) is an object override,
        // applied later from the full argv; accept it here.
        if (key.find('.') != std::string_view::npos)
        {
            continue;
        }
        if (key == "config")
        {
            if (hasValue)
            {
                cl.configOption = std::string(body.substr(eq + 1));
            }
            else if (i + 1 < argc)
            {
                cl.configOption = argv[++i];
            }
            continue;
        }
        auto const* info = findInfo(key);
        if (info == nullptr)
        {
            cl.unknownOptions.emplace_back(arg);
            continue;
        }
        // Consume the value token of `--opt value` so it is not read as a
        // positional input; the value is applied later by Config::load_file.
        if (info->takesValue && !hasValue && i + 1 < argc)
        {
            ++i;
        }
    }
    return cl;
}

//------------------------------------------------
//
// Generate action
//
//------------------------------------------------

static
Expected<void>
DoGenerateAction(
    std::string const& configPath,
    ReferenceDirectories const& dirs,
    char const** argv)
{
    // --------------------------------------------------------------
    //
    // Load configuration
    //
    // --------------------------------------------------------------
    Config config;
    MRDOCS_TRY(Config::load_file(config, configPath, dirs, argv));

    // --------------------------------------------------------------
    //
    // Load plugins
    //
    // --------------------------------------------------------------
    // Plugins come first: one that cannot install what it provides
    // fails the run, while an addon generator directory whose id is
    // already taken is skipped.
    MRDOCS_TRY(loadPlugins(config));

    // --------------------------------------------------------------
    //
    // Discover addon-defined generators
    //
    // --------------------------------------------------------------
    // Register the data-driven Handlebars generators contributed by
    // addons so they resolve alongside the built-ins when a generator is
    // selected below.
    MRDOCS_TRY(hbs::discoverDataDrivenGenerators(config));

    MRDOCS_CHECK(config.output, "The output path argument is missing");
    MRDOCS_CHECK(
        !config.generator.values.empty(), "No generator was specified");

    // --------------------------------------------------------------
    //
    // Build corpus
    //
    // --------------------------------------------------------------
    // Corpus::build resolves the compilation database from the
    // configuration (a compile_commands.json path, a CMakeLists.txt plus
    // cmake options, or one synthesized from source-root and input). A
    // corpus is just extracted symbols; extensions are a separate step.
    MRDOCS_TRY(Corpus corpus, Corpus::build(config));
    // The global namespace is always extracted, so a size of 1 means no
    // declaration other than the global namespace was found. Treat that
    // the same as a truly empty corpus here.
    if (corpus.size() <= 1)
    {
        if (config.errorOnEmptyCorpus)
        {
            report::error("Corpus is empty, not generating docs");
            return Unexpected(Error("Corpus is empty, not generating docs"));
        }
        report::warn("Corpus is empty, not generating docs");
        return {};
    }

    // --------------------------------------------------------------
    //
    // Apply plugin transforms
    //
    // --------------------------------------------------------------
    // Plugin transforms run first, which is what this call site ahead of
    // the extension one decides: what MrDocs was set up with applies
    // before what a user script asks for.
    MRDOCS_TRY(applyTransforms(corpus, config));

    // --------------------------------------------------------------
    //
    // Run user extension scripts
    //
    // --------------------------------------------------------------
    // Extensions are optional and live outside the corpus. Load them,
    // then apply their transforms after finalization and before any
    // generator runs, so mutations are visible to every output format.
    // The registry also owns the script-defined generators, used below.
    MRDOCS_TRY(ExtensionRegistry extensions, ExtensionRegistry::load(config));
    MRDOCS_TRY(extensions.applyTransforms(corpus, config));

    // --------------------------------------------------------------
    //
    // Generate docs
    //
    // --------------------------------------------------------------
    // The tool owns this order. Every generator is a Generator, whether
    // built-in, data-driven, or script-defined, so each is looked up and
    // built the same way. A script-defined generator takes precedence over
    // a built-in or data-driven one of the same id.
    report::info("Generating docs");
    for (std::string const& genId : config.generator.values)
    {
        using clock_type = std::chrono::steady_clock;
        auto const start_time = clock_type::now();
        Generator const* generator = extensions.findGenerator(genId);
        if (generator == nullptr)
        {
            generator = findGenerator(genId);
        }
        MRDOCS_CHECK(
            generator,
            formatError("the Generator \"{}\" was not found", genId));
        MRDOCS_TRY(generator->build(corpus, config));
        report::info(
            "Generated {} documentation in {}",
            genId,
            format_duration(clock_type::now() - start_time));
    }

    return {};
}

//------------------------------------------------
//
// Entry point
//
//------------------------------------------------

static
Expected<ReferenceDirectories>
getReferenceDirectories(std::string const& execPath)
{
    ReferenceDirectories dirs;
    dirs.mrdocsRoot = files::getParentDir(execPath, 2);
    llvm::SmallVector<char, 256> cwd;
    if (auto ec = llvm::sys::fs::current_path(cwd); ec)
    {
        return Unexpected(formatError("Unable to determine current working directory: {}", ec.message()));
    }
    dirs.cwd = std::string(cwd.data(), cwd.size());
    return dirs;
}

static
Expected<std::string>
getConfigPath(ReferenceDirectories const& dirs, CommandLine const& cl)
{
    std::string configPath;
    if (!cl.configOption.empty())
    {
        // From explicit --config argument
        configPath = cl.configOption;
    }
    else if (auto const it = std::ranges::find_if(cl.inputs,
                 [](std::string_view const p)
                 {
                     return files::getFileName(p) == "mrdocs.yml";
                 });
             it != cl.inputs.end())
    {
        // From implicit command line inputs
        configPath = *it;
    }
    else if (files::exists("./mrdocs.yml"))
    {
        // From current directory
        configPath = "./mrdocs.yml";
    }
    else
    {
        return Unexpected(formatError("The config path is missing"));
    }
    configPath = files::makeAbsolute(configPath, dirs.cwd);
    return configPath;
}

int
mrdocs_main(int argc, char const** argv)
{
    // Enable stack traces
    llvm::EnablePrettyStackTrace();
    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
    llvm::setBugReportMsg("PLEASE submit a bug report to https://github.com/cppalliance/mrdocs/issues/ and include the crash backtrace.\n");

    // Set up addons directory
#ifdef __GNUC__
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wpedantic"
    // error: ISO C++ forbids taking address of function ‘::main’
#endif
    void* addressOfMain = reinterpret_cast<void*>(&main);
#ifdef __GNUC__
#    pragma GCC diagnostic pop
#endif
    std::string execPath = llvm::sys::fs::
        getMainExecutable(argv[0], addressOfMain);

    // Parse the command line straight from argv against the config schema.
    CommandLine const cl = parseCommandLine(argc, argv);
    if (cl.showHelp)
    {
        printHelp(llvm::outs());
        return EXIT_SUCCESS;
    }
    if (cl.showVersion)
    {
        printVersion(llvm::outs(), execPath);
        return EXIT_SUCCESS;
    }
    if (!cl.unknownOptions.empty())
    {
        for (std::string const& opt : cl.unknownOptions)
        {
            report::error("Unknown option: {} (use --help to list options)", opt);
        }
        return EXIT_FAILURE;
    }

    // Before `DoGenerateAction`, we use an error reporting level.
    // DoGenerateAction will set the level to whatever is specified in
    // the command line or the configuration file
    report::setMinimumLevel(report::Level::error);
    auto res = getReferenceDirectories(execPath);
    if (!res)
    {
        report::fatal("Failed to determine reference directories: {}", res.error().message());
        return EXIT_FAILURE;
    }
    auto dirs = *std::move(res);

    auto expConfigPath = getConfigPath(dirs, cl);
    if (!expConfigPath)
    {
        report::fatal("Failed to determine config path: {}", expConfigPath.error().message());
        return EXIT_FAILURE;
    }
    auto configPath = *std::move(expConfigPath);

    // Generate
    if (auto exp = DoGenerateAction(configPath, dirs, argv); !exp)
    {
        report::error("Generating reference failed: {}", exp.error());
    }
    if (report::results.errorCount > 0 ||
        report::results.fatalCount > 0)
    {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

#ifdef _NDEBUG
static
void
reportUnhandledException(
    std::exception const& ex)
{
    namespace sys = llvm::sys;

    report::fatal("Unhandled exception: {}\n", ex.what());
    sys::PrintStackTrace(llvm::errs());
}
#endif

} // mrdocs

int
main(int argc, char const** argv)
{
#ifndef _NDEBUG
    return mrdocs::mrdocs_main(argc, argv);
#else
    try
    {
        return mrdocs::mrdocs_main(argc, argv);
    }
    catch(mrdocs::Exception const& ex)
    {
        // Thrown Exception should never get here.
        mrdocs::reportUnhandledException(ex);
    }
    catch(std::exception const& ex)
    {
        mrdocs::reportUnhandledException(ex);
    }
    return EXIT_FAILURE;
#endif
}
