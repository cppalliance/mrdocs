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

#include "ClangDoc.h"
#include "Generators.h"
#include "Representation.h"
#include <mrdox/Corpus.hpp>
#include <mrdox/Errors.hpp>
#include <clang/AST/AST.h>
#include <clang/AST/Decl.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchersInternal.h>
#include <clang/Driver/Options.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/AllTUsExecution.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/JSONCompilationDatabase.h>
#include <clang/Tooling/Execution.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/Process.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/raw_ostream.h>
#include <memory>
#include <string>

// VFALCO GARBAGE
extern void force_xml_generator_linkage();

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

enum OutputFormatTy
{
    adoc,
    xml
};

static
llvm::cl::opt<OutputFormatTy>
FormatEnum(
    "format",
    llvm::cl::desc("Format for outputted docs."),
    llvm::cl::values(
        clEnumValN(OutputFormatTy::adoc, "adoc",
            "Documentation in Asciidoc format."),
        clEnumValN(OutputFormatTy::xml, "xml",
            "Documentation in XML format.")),
    llvm::cl::init(OutputFormatTy::adoc), // default value
    llvm::cl::cat(ToolCategory));

std::string
getFormatString()
{
    switch (FormatEnum)
    {
    case OutputFormatTy::adoc:
        return "adoc";
    case OutputFormatTy::xml:
        return "xml";
    }
    llvm_unreachable("Unknown OutputFormatTy");
}

} // (anon)

//------------------------------------------------

int
toolMain(int argc, const char** argv)
{
    // VFALCO GARBAGE
    force_xml_generator_linkage();

    Config cfg;
    Reporter R;
    ErrorCode ec;

    // parse command line options
    {
        Result rv = tooling::CommonOptionsParser::create(
            argc, argv, ToolCategory, llvm::cl::OneOrMore, Overview);
        if(! rv)
        {
            R.fail("CommonOptionsParser::create", rv);
            return EXIT_FAILURE;
        }
        cfg.options = std::make_unique<tooling::CommonOptionsParser>(std::move(*rv));
    }

    /*
    if (! DoxygenOnly)
        cfg.ArgAdjuster = tooling::combineAdjusters(
            getInsertArgumentAdjuster(
                "-fparse-all-comments",
                tooling::ArgumentInsertPosition::END),
            cfg.ArgAdjuster);
    */

    cfg.PublicOnly = true;
    cfg.OutDirectory = OutDirectory;
    cfg.IgnoreMappingFailures = IgnoreMappingFailures;

    // create the executor
    auto ex = std::make_unique<tooling::AllTUsToolExecutor>(
        cfg.options->getCompilations(), 0);

    // create the generator
    std::unique_ptr<Generator> G;
    {
        std::string Format = getFormatString();
        Result rv = findGeneratorByName(Format);
        if(! rv)
        {
            R.fail("findGeneratorByName", rv);
            return false;
        }
        G = std::move(*rv);
    }

    auto rv = buildCorpus(*ex, cfg, R);
    if(! rv)
        return EXIT_FAILURE;

    //--------------------------------------------

    // Ensure the root output directory exists.
    if (std::error_code Err =
            llvm::sys::fs::create_directories(cfg.OutDirectory);
                Err != std::error_code())
    {
        llvm::errs() << "Failed to create directory '" << cfg.OutDirectory << "'\n";
        return EXIT_FAILURE;
    }

    // Run the generator.
    llvm::outs() << "Generating docs...\n";
    if(auto Err = G->generateDocs(
        cfg.OutDirectory,
        *rv,
        cfg))
    {
        llvm::errs() << toString(std::move(Err)) << "\n";
        return EXIT_FAILURE;
    }

    //
    // Generate assets
    //
    {
        llvm::outs() << "Generating assets for docs...\n";
        auto Err = G->createResources(cfg, *rv);
        if (Err) {
            llvm::errs() << toString(std::move(Err)) << "\n";
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

} // mrdox
} // clang

//------------------------------------------------

int main(int argc, char const** argv)
{
    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
    return clang::mrdox::toolMain(argc, argv);
}
