//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "ToolArgs.hpp"
#include "Addons.hpp"
#include <lib/Support/Path.hpp>
#include <cstddef>
#include <vector>
#include <string>
#include <ranges>

namespace clang {
namespace mrdocs {

ToolArgs ToolArgs::instance_;

//------------------------------------------------

ToolArgs::
ToolArgs()
    : cmdLineCat("Command Line")
    , pathsCat("Paths")
    , buildOptsCat("Build Options")
    , generatorCat("Generators")
    , filtersCat("Filters")
    , extraCat("Extra")

    , usageText(
R"( Generate C++ reference documentation
)")

    , extraHelp(
R"(
EXAMPLES:
    mrdocs
    mrdocs docs/mrdocs.yml
    mrdocs docs/mrdocs.yml ../build/compile_commands.json
)")
, inputs(llvm::cl::Sink, llvm::cl::cat(cmdLineCat))
, config(llvm::cl::init("./mrdocs.yml"), llvm::cl::cat(cmdLineCat))
, concurrency(llvm::cl::init(0), llvm::cl::cat(cmdLineCat))
, sourceRoot(llvm::cl::cat(pathsCat))
, output(llvm::cl::cat(pathsCat))
, compilationDatabase(llvm::cl::cat(pathsCat))
, cmake(llvm::cl::cat(buildOptsCat))
, defines(llvm::cl::cat(buildOptsCat))
, generate(llvm::cl::init("adoc"), llvm::cl::cat(generatorCat))
, addons(llvm::cl::cat(generatorCat))
, multipage(llvm::cl::init(true), llvm::cl::cat(generatorCat))
, baseURL(llvm::cl::cat(generatorCat))
, referencedDeclarations(llvm::cl::init("dependency"), llvm::cl::cat(filtersCat))
, anonymousNamespaces(llvm::cl::init("always"), llvm::cl::cat(filtersCat))
, inaccessibleMembers(llvm::cl::init("always"), llvm::cl::cat(filtersCat))
, inaccessibleBases(llvm::cl::init("always"), llvm::cl::cat(filtersCat))
, input(filtersCat)
, filters(filtersCat)
, seeBelow(llvm::cl::cat(filtersCat))
, implementationDefined(llvm::cl::cat(filtersCat))
, verbose(llvm::cl::cat(extraCat))
, report(llvm::cl::init(1), llvm::cl::cat(extraCat))
, ignoreMapErrors(llvm::cl::cat(extraCat))
, ignoreFailures(llvm::cl::cat(extraCat))
{}

ToolArgs::
FileFilter::
FileFilter(llvm::cl::OptionCategory& cat)
: include(llvm::cl::cat(cat))
, filePatterns(llvm::cl::cat(cat))
{}

ToolArgs::
Filters::
Filters(llvm::cl::OptionCategory& cat)
: symbols(cat)
{}

ToolArgs::
Filters::
Category::
Category(llvm::cl::OptionCategory& cat)
: include(llvm::cl::cat(cat))
, exclude(llvm::cl::cat(cat))
{}

void
ToolArgs::
hideForeignOptions()
{
    #define OPTION(Name, Kebab, Desc) \
    Name.setArgStr(#Kebab);           \
    Name.setDescription(#Desc);
    #include <mrdocs/ConfigOptions.inc>

    // VFALCO When adding an option, it must
    // also be added to this list or else it
    // will stay hidden.
    std::vector<llvm::cl::Option const*> ours({
        #define OPTION(Name, Kebab, Desc) std::addressof(Name),
        #include <mrdocs/ConfigOptions.inc>
    });

    // Really hide the clang/llvm default
    // options which we didn't ask for.
    auto optionMap = llvm::cl::getRegisteredOptions();
    for(auto& opt : optionMap)
    {
        if(std::ranges::find(ours, opt.getValue()) != ours.end())
            opt.getValue()->setHiddenFlag(llvm::cl::NotHidden);
        else
            opt.getValue()->setHiddenFlag(llvm::cl::ReallyHidden);
    }
}

namespace
{
template <class T1, class T2>
void
applyOpt(T1& dest, llvm::cl::opt<T2>& opt)
{
    dest = opt;
}

template <class T1, class T2>
void
applyOpt(T1& dest, llvm::cl::list<T2>& opt)
{
    dest = opt;
}

void
applyOpt(
    Config::Settings::ExtractPolicy& dest,
    llvm::cl::opt<std::string>& opt)
{
    using ExtractPolicy = Config::Settings::ExtractPolicy;
    constexpr std::array<std::pair<std::string_view, ExtractPolicy>, 3>
        extractPolicyOptions = {
            std::pair("always", ExtractPolicy::Always),
            std::pair("dependency", ExtractPolicy::Dependency),
            std::pair("never", ExtractPolicy::Never)
        };
    auto policyIt = std::ranges::find(
        extractPolicyOptions,
        opt.getValue(),
        &std::pair<std::string_view, ExtractPolicy>::first);
    if (policyIt != extractPolicyOptions.end())
    {
        dest = policyIt->second;
    }
    else
    {
        report::warn("Invalid policy value: '{}'", opt.getValue());
    }
}

bool
keyIsSet(std::string_view key, char const** argv, char const** argvEnd)
{
    namespace stdv = std::ranges::views;
    auto toSV = [](auto arg) { return std::string_view(arg); };
    auto isKey = [](std::string_view arg) { return arg.starts_with("--"); };
    auto getKey = [](std::string_view arg) { return arg.substr(2, arg.find_first_of('=') - 2); };
    auto keys =
        std::ranges::subrange(argv, argvEnd) |
        stdv::transform(toSV) |
        stdv::filter(isKey) |
        stdv::transform(getKey);
    return std::ranges::find(keys, key) != keys.end();
}

void
overrideCommonOptions(
    Config::Settings& settings,
    ToolArgs& args,
    char const** argv)
{
    auto argv_end = argv;
    for (; *argv_end; ++argv_end);
#define COMMON_OPTION(Name, Kebab, Desc)        \
    if (keyIsSet(#Kebab, argv + 1, argv_end)) { \
        applyOpt(settings.Name, args.Name);     \
    }
    #include <mrdocs/ConfigOptions.inc>
}

}

Expected<void>
ToolArgs::
apply(
    Config::Settings& s,
    std::string_view execPath,
    char const** argv)
{
    namespace stdr = std::ranges;
    namespace stdv = std::views;

    // Set and load config file
    auto filenames = stdv::transform(inputs, files::getFileName);
    if (!toolArgs.config.isDefaultOption())
    {
        s.config = toolArgs.config.getValue();
    }
    else if (
        auto configInput = stdr::find(filenames, "mrdocs.yml");
        configInput != filenames.end())
    {
        s.config = *configInput.base();
    }
    else
    {
        s.config = toolArgs.config.getValue();
    }
    MRDOCS_CHECK(s.config, "The config mrdocs.yml path is missing");
    MRDOCS_CHECK(files::exists(s.config), "The config mrdocs.yml path does not exist");
    MRDOCS_CHECK(s.config, "The config mrdocs.yml path is missing");
    MRDOCS_TRY(s.config, files::makeAbsolute(s.config));
    s.config = files::normalizePath(s.config);
    s.configDir = files::normalizeDir(files::getParentDir(s.config));
    MRDOCS_TRY(s.configYaml, files::getFileText(s.config));
    MRDOCS_TRY(loadConfig(s, s.configYaml));

    // Override common options with args from the command line
    overrideCommonOptions(s, toolArgs, argv);

    // Override common options with inputs from command line
    constexpr std::array<std::string_view, 2> compilationDatabaseFilenames = {
        "compile_commands.json",
        "CMakeLists.txt"
    };
    for (auto const& cmdLineInput: inputs)
    {
        // Input is mrdocs.yml
        auto filename = files::getFileName(cmdLineInput);
        if (filename == "mrdocs.yml")
        {
            continue;
        }
        // Input is compilation database
        if (stdr::find(compilationDatabaseFilenames, filename) !=
            compilationDatabaseFilenames.end())
        {
            s.compilationDatabase = files::makeAbsolute(cmdLineInput, s.configDir);
            continue;
        }
        // Input is source root
        auto absPath = files::makeAbsolute(cmdLineInput, s.configDir);
        if (files::isDirectory(cmdLineInput))
        {
            s.sourceRoot = cmdLineInput;
            continue;
        }
        // Input category is unknown
        report::warn("Ignoring unknown input path: {}", cmdLineInput);
    }
    s.sourceRoot = files::normalizeDir(files::makeAbsolute(s.sourceRoot, s.configDir));

    // Set compilation database from source root
    if (s.compilationDatabase.empty() && !s.sourceRoot.empty())
    {
        for (auto const& filename: compilationDatabaseFilenames)
        {
            auto cd = files::makeAbsolute(filename, s.sourceRoot);
            if (files::exists(cd))
            {
                s.compilationDatabase = cd;
                break;
            }
        }
    }

    // Set the addons directory relative to binaries if not set yet
    if (s.addons.empty())
    {
        auto exp = setupAddonsDir(s.addons, execPath);
        if (!exp)
        {
            return formatError(
                "{}: \"{}\"\n"
                "Could not locate the addons directory because "
                "the MRDOCS_ADDONS_DIR environment variable is not set, "
                "no valid addons location was specified on the command line, "
                "and no addons directory exists in the same directory as "
                "the executable.",
                exp.error(), toolArgs.addons);
        }
    }

    // Make all common relative paths relative to the config file and
    // make all directories dirsy
    auto makeRelativeToConfigDir = [&](
        std::string& path,
        bool createIfNeeded) -> Expected<void>
    {
        if (!path.empty())
        {
            path = files::makeAbsolute(path, s.configDir);
        }
        MRDOCS_CHECK(
            (createIfNeeded || path.empty() || files::exists(path)),
            formatError("Path {} does not exist", path));
        path = files::normalizePath(path);
        if (createIfNeeded)
        {
            files::createDirectory(output);
        }
        if (files::isDirectory(path)) { path = files::makeDirsy(path); }
        return {};
    };
    MRDOCS_TRY(makeRelativeToConfigDir(s.addons, false));
    MRDOCS_TRY(makeRelativeToConfigDir(s.sourceRoot, false));
    MRDOCS_TRY(makeRelativeToConfigDir(s.output, true));
    MRDOCS_TRY(makeRelativeToConfigDir(s.compilationDatabase, false));
    for (auto& include: s.input.include)
    {
        MRDOCS_TRY(makeRelativeToConfigDir(include, false));
    }

    return {};
}

} // mrdocs
} // clang
