//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

// This tool for generating C and C++ documentation from source code
// and comments. Generally, it runs a LibTooling FrontendAction on source files,
// mapping each declaration in those files to its USR and serializing relevant
// information into LLVM bitcode. It then runs a pass over the collected
// declaration information, reducing by USR. There is an option to dump this
// intermediate result to bitcode. Finally, it hands the reduced information
// off to a generator, which does the final parsing from the intermediate
// representation to the desired output format.
//

#include <mrdox/Config.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/Errors.hpp>
#include <mrdox/Generator.hpp>
#include <clang/Tooling/AllTUsExecution.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Signals.h>

namespace clang {
namespace mrdox {

//------------------------------------------------

namespace {

const char* Overview =
R"(Generates documentation from source code and comments.

Examples

  $ mrdox mrdox.yml
  $ mrdox --output ./docs mrdox.yml
)";

static
llvm::cl::extrahelp
CommonHelp(
    tooling::CommonOptionsParser::HelpMessage);

static
llvm::cl::OptionCategory
ToolCategory("mrdox options");

static
llvm::cl::opt<bool>
    IgnoreMappingFailures(
        "ignore-map-errors",
        llvm::cl::desc("Continue if files are not mapped correctly."),
        llvm::cl::init(true),
        llvm::cl::cat(ToolCategory));

static
llvm::cl::opt<std::string>
OutDirectory(
    "output",
    llvm::cl::desc("Directory for outputting generated files."),
    llvm::cl::init("."),
    llvm::cl::cat(ToolCategory));

static
llvm::cl::opt<std::string>
    ConfigPath(
    "config-file",
    llvm::cl::desc(R"(The config filename relative to the repository root)"),
    llvm::cl::init("mrdox.yaml"),
    llvm::cl::cat(ToolCategory));

static
llvm::cl::opt<std::string>
FormatType(
    "format",
    llvm::cl::desc("Format for outputted docs (\"adoc\" or \"xml\")."),
    llvm::cl::init("adoc"),
    llvm::cl::cat(ToolCategory));

} // (anon)

//------------------------------------------------

int
toolMain(int argc, const char** argv)
{
    std::vector<std::unique_ptr<Generator>> formats;
    formats.emplace_back(makeXMLGenerator());
    formats.emplace_back(makeAsciidocGenerator());

    Config config;
    Reporter R;

    // parse command line options
    auto optionsResult = tooling::CommonOptionsParser::create(
        argc, argv, ToolCategory, llvm::cl::OneOrMore, Overview);
    if(R.failed("CommonOptionsParser::create", optionsResult))
        return EXIT_FAILURE;

    /*
    if (! DoxygenOnly)
        config.ArgAdjuster = tooling::combineAdjusters(
            getInsertArgumentAdjuster(
                "-fparse-all-comments",
                tooling::ArgumentInsertPosition::END),
            config.ArgAdjuster);
    */

    config.PublicOnly = true;
    config.OutDirectory = OutDirectory;
    config.IgnoreMappingFailures = IgnoreMappingFailures;

    config.load(ConfigPath);

    // create the executor
    auto ex = std::make_unique<tooling::AllTUsToolExecutor>(
        optionsResult->getCompilations(), 0);

    // create the generator
    Generator const* gen;
    {
        auto it = std::find_if(
            formats.begin(), formats.end(),
            [](std::unique_ptr<Generator> const& up)
            {
                return up->extension().equals_insensitive(FormatType.getValue());
            });
        if(it == formats.end())
        {
            llvm::Error err = llvm::createStringError(
                llvm::inconvertibleErrorCode(),
                "unknown format");
            R.failed("FormatType", err);
            return R.getExitCode();
        }
        gen = it->get();
    }

    // Run the tool, this can take a while
    auto rv = Corpus::build(*ex, config, R);
    if(! rv)
        return EXIT_FAILURE;

    // Run the generator.
    llvm::outs() << "Generating docs...\n";
    if(! gen->build(config.OutDirectory, *rv, config, R))
        return EXIT_FAILURE;

    return R.getExitCode();
}

} // mrdox
} // clang

//------------------------------------------------

int main(int argc, char const** argv)
{
    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
    return clang::mrdox::toolMain(argc, argv);
}
