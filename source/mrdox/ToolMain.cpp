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
#include <mrdox/Debug.hpp>
#include <mrdox/Generators.hpp>
#include <mrdox/Reporter.hpp>
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
  $ mrdox --config=mrdox.yml --output ./docs
)";

static
llvm::cl::extrahelp
CommonHelp(
    tooling::CommonOptionsParser::HelpMessage);

static
llvm::cl::OptionCategory
ToolCategory("mrdox options");

static
llvm::cl::opt<std::string>
ConfigPath(
    "config",
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

static
llvm::cl::opt<bool>
IgnoreMappingFailures(
    "ignore-map-errors",
    llvm::cl::desc("Continue if files are not mapped correctly."),
    llvm::cl::init(true),
    llvm::cl::cat(ToolCategory));

static
llvm::cl::opt<std::string>
OutputPath(
    "output",
    llvm::cl::desc("Directory or file for generating output."),
    llvm::cl::init("."),
    llvm::cl::cat(ToolCategory));

} // (anon)

//------------------------------------------------

void
toolMain(
    int argc, const char** argv,
    Reporter& R)
{
    auto& generators = getGenerators();

    // parse command line options
    auto optionsResult = tooling::CommonOptionsParser::create(
        argc, argv, ToolCategory, llvm::cl::OneOrMore, Overview);
    if(R.error(optionsResult, "calculate command line options"))
        return;

    auto config = Config::loadFromFile(ConfigPath);
    if(! config)
        return (void)R.error(config, "load config file '", ConfigPath, "'");

    (*config)->setOutputPath(OutputPath);
    (*config)->IgnoreMappingFailures = IgnoreMappingFailures;

    // create the executor
    auto ex = std::make_unique<tooling::AllTUsToolExecutor>(
        optionsResult->getCompilations(), 0);

    // create the generator
    auto generator = generators.find(FormatType.getValue());
    if(! generator)
    {
        R.print("Generator '", FormatType.getValue(), "' not found.");
        return;
    }

    // Run the tool, this can take a while
    auto corpus = Corpus::build(*ex, *config, R);
    if(R.error(corpus, "build the documentation corpus"))
        return;

    // Run the generator.
    if((*config)->verbose())
        llvm::outs() << "Generating docs...\n";
    if((*corpus)->config()->singlePage())
    {
        auto err = generator->buildSinglePageFile(
            (*config)->outputPath(), **corpus, R);
        if(R.error(err,
            "generate '", (*config)->outputPath(), "'"))
        {
        }
    }
    else
    {
        auto err = generator->buildPages(
            (*config)->outputPath(), **corpus, R);
        if(R.error(err,
            "generate pages in '", (*config)->outputPath(), "'"))
        {
        }
    }
}

} // mrdox
} // clang

//------------------------------------------------

int main(int argc, char const** argv)
{
    using namespace clang::mrdox;

    debugEnableHeapChecking();
    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);

    Reporter R;
    toolMain(argc, argv, R);
    return R.getExitCode();
}
