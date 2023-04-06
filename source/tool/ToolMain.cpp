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
#include <mrdox/ErrorCode.hpp>
#include <mrdox/Reporter.hpp>
#include <clang/AST/AST.h>
#include <clang/AST/Decl.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchersInternal.h>
#include <clang/Driver/Options.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/CommonOptionsParser.h>
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
#include <atomic>
#include <mutex>
#include <string>

// VFALCO GARBAGE
extern void force_xml_generator_linkage();

namespace clang {
namespace mrdox {

//------------------------------------------------

namespace {

const char* Overview =
R"(Generates documentation from source code and comments.

Example usage for files without flags (default):

  $ mrdox File1.cpp File2.cpp ... FileN.cpp

Example usage for a project using a compile commands database:

  $ mrdox --executor=all-TUs compile_commands.json
)";

static
llvm::cl::extrahelp
CommonHelp(
    tooling::CommonOptionsParser::HelpMessage);

static
llvm::cl::OptionCategory
MrDoxCategory("mrdox options");

static
llvm::cl::opt<std::string>
ProjectName(
    "project-name",
    llvm::cl::desc("Name of project."),
    llvm::cl::cat(MrDoxCategory));

static
llvm::cl::opt<bool>
    IgnoreMappingFailures(
        "ignore-map-errors",
        llvm::cl::desc("Continue if files are not mapped correctly."),
        llvm::cl::init(true),
        llvm::cl::cat(MrDoxCategory));

static
llvm::cl::opt<std::string>
OutDirectory(
    "output",
    llvm::cl::desc("Directory for outputting generated files."),
    llvm::cl::init("docs"),
    llvm::cl::cat(MrDoxCategory));

static
llvm::cl::opt<bool>
PublicOnly(
    "public",
    llvm::cl::desc("Document only public declarations."),
    llvm::cl::init(false),
    llvm::cl::cat(MrDoxCategory));

static
llvm::cl::opt<std::string>
SourceRoot(
    "source-root", llvm::cl::desc(R"(
Directory where processed files are stored.
Links to definition locations will only be
generated if the file is in this dir.)"),
    llvm::cl::cat(MrDoxCategory));

static
llvm::cl::opt<std::string>
RepositoryUrl(
    "repository", llvm::cl::desc(R"(
URL of repository that hosts code.
Used for links to definition locations.)"),
    llvm::cl::cat(MrDoxCategory));

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
    llvm::cl::cat(MrDoxCategory));

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

bool
setupConfig(
    Config& cfg,
    llvm::SmallVectorImpl<llvm::StringRef> const& args,
    Reporter& R)
{
    llvm::SmallVector<char const*> argv;
    for(auto const& s : args)
        argv.push_back(s.data());
    int argc = argv.size();
    if(llvm::Error err = clang::tooling::createExecutorFromCommandLineArgs(
        argc,
        argv.begin(),
        MrDoxCategory,
        Overview).moveInto(cfg.Executor))
    {
        return false;
    }

    // Fail early if an invalid format was provided.
    std::string Format = getFormatString();
    llvm::outs() << "Emitting docs in " << Format << " format.\n";
    if(llvm::Error err =
        mrdox::findGeneratorByName(Format).moveInto(cfg.G))
    {
        return false;
    }

    /*
    if (! DoxygenOnly)
        cfg.ArgAdjuster = tooling::combineAdjusters(
            getInsertArgumentAdjuster(
                "-fparse-all-comments",
                tooling::ArgumentInsertPosition::END),
            cfg.ArgAdjuster);
    */

    cfg.ProjectName = ProjectName;
    cfg.PublicOnly = PublicOnly;
    cfg.OutDirectory = OutDirectory;
    cfg.SourceRoot = SourceRoot;
    cfg.RepositoryUrl = RepositoryUrl;
    cfg.IgnoreMappingFailures = IgnoreMappingFailures;
    cfg.OutDirectory = OutDirectory;

    return true;
}

bool
setupConfig(
    Config& cfg,
    int argc, const char** argv,
    Reporter& R)
{
    llvm::SmallVector<llvm::StringRef, 16> args;
    for(int i = 0; i < argc; ++i)
        args.push_back(argv[i]);
    return setupConfig(cfg, args, R);
}

//------------------------------------------------

int
toolMain(int argc, const char** argv)
{
    // VFALCO GARBAGE
    force_xml_generator_linkage();

    Config cfg;
    Corpus corpus;
    Reporter R;
    ErrorCode ec;

    if(! setupConfig(cfg, argc, argv, R))
        return EXIT_FAILURE;

    // Extract the AST first
    if(llvm::Error err = doMapping(corpus, cfg))
    {
        llvm::errs() <<
            toString(std::move(err)) << "\n";
        return EXIT_FAILURE;
    }

    // Build the internal representation of
    // the C++ declarations to be documented.
    if(llvm::Error err = buildIndex(corpus, cfg))
    {
        llvm::errs() <<
            toString(std::move(err)) << "\n";
        return EXIT_FAILURE;
    }

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
    if(auto Err = cfg.G->generateDocs(
        cfg.OutDirectory,
        corpus,
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
        auto Err = cfg.G->createResources(cfg, corpus);
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
