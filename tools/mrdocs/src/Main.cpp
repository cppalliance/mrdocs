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
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Extensions/ExtensionRegistry.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Support/Chrono.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <mrdocs/Support/Report.hpp>
#include <mrdocs/Version.hpp>
#include <tool/PublicToolArgs.hpp>
#include <llvm/Support/CommandLine.h>
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

// The tool's command-line options are exactly the generated PublicToolArgs; the
// tool adds no options of its own, so it uses a single PublicToolArgs instance
// directly rather than a subclass. llvm::cl registers options globally on
// construction, so exactly one instance may exist.
static PublicToolArgs toolArgs;

static char const* const usageText = "Generate C++ reference documentation";

static llvm::cl::extrahelp toolExtraHelp(
R"(
EXAMPLES:
    mrdocs
    mrdocs docs/mrdocs.yml
    mrdocs docs/mrdocs.yml ../build/compile_commands.json
)");

// Whether `arg` is a `--<name>.<...>` override for one of the object
// options (a `map<string,object>` config option). Those nested keys have no
// registered llvm::cl flag, so they must be hidden from the parser and are
// applied later by PublicToolArgs::apply from the original argv. This is a
// tool-only, temporary concern: kept here rather than in the library.
static bool
isObjectOverrideArg(
    std::string_view arg,
    std::span<std::string_view const> objectOptionNames)
{
    if (!arg.starts_with("--"))
    {
        return false;
    }
    std::string_view key = arg.substr(2);
    key = key.substr(0, key.find('='));
    auto const dot = key.find('.');
    if (dot == std::string_view::npos)
    {
        return false;
    }
    std::string_view const head = key.substr(0, dot);
    return std::ranges::find(objectOptionNames, head) != objectOptionNames.end();
}

// Return argv without the dotted object-override arguments; argv[0] is kept.
static std::vector<char const*>
filterCommandLine(int argc, char const** argv)
{
    std::vector<char const*> result;
    result.reserve(static_cast<std::size_t>(argc));
    auto const names = PublicToolArgs::objectOptionNames();
    for (int i = 0; i < argc; ++i)
    {
        if (i > 0 && isObjectOverrideArg(argv[i], names))
        {
            continue;
        }
        result.push_back(argv[i]);
    }
    return result;
}

// Hide every registered llvm::cl option that is not one of ours.
static void
hideForeignOptions()
{
    std::vector<llvm::cl::Option const*> oursOptions;
    toolArgs.visit([&](std::string_view, auto const& opt)
    {
        oursOptions.push_back(std::addressof(opt));
    });
    auto optionMap = llvm::cl::getRegisteredOptions();
    for (auto& opt : optionMap)
    {
        opt.second->setHiddenFlag(
            std::ranges::find(oursOptions, opt.second) != oursOptions.end() ?
            llvm::cl::NotHidden :
            llvm::cl::ReallyHidden);
    }
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
    MRDOCS_TRY(Config::load_file(config, configPath));
    MRDOCS_TRY(toolArgs.apply(config, dirs, argv));
    MRDOCS_TRY(config.normalize(dirs));
    report::setMinimumLevel(static_cast<report::Level>(config.logLevel));

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
getConfigPath(ReferenceDirectories const& dirs)
{
    std::string configPath;
    auto cmdLineFilenames = std::ranges::views::transform(
        toolArgs.cmdLineInputs, files::getFileName);
    if (!toolArgs.config.getValue().empty())
    {
        // From explicit --config argument
        configPath = toolArgs.config.getValue();
    }
    else if (auto const it = std::ranges::find(cmdLineFilenames, "mrdocs.yml");
             it != cmdLineFilenames.end())
    {
        // From implicit command line inputs
        configPath = *(it.base());
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

    // Parse command line options
    llvm::cl::SetVersionPrinter([execPath](llvm::raw_ostream& os) {
        os << project_name << " version " << project_version_with_build << "\n";
        os << "Built with LLVM " << LLVM_VERSION_STRING << "\n";
        os << "Build SHA: " << project_version_build << "\n";
        os << "Target: " << llvm::sys::getDefaultTargetTriple() << "\n";
        os << "InstalledDir: " << files::getParentDir(execPath) << "\n";
    });

    hideForeignOptions();

    // Dotted overrides for object options (--<name>.<key>.<field>=<value>)
    // address dynamic keys with no registered flag, so they are hidden from
    // the llvm::cl parser here and applied later from the original argv.
    std::vector<char const*> parsedArgv = filterCommandLine(argc, argv);

    if (!llvm::cl::ParseCommandLineOptions(
        static_cast<int>(parsedArgv.size()),
        parsedArgv.data(), usageText))
    {
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

    auto expConfigPath = getConfigPath(dirs);
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
