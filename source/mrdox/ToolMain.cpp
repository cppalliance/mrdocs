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

using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace clang;

// VFALCO GARBAGE
extern void force_xml_generator_linkage();

// This function isn't referenced outside its translation unit, but it
// can't use the "static" keyword because its address is used for
// GetMainExecutable (since some platforms don't support taking the
// address of main, and some platforms can't implement GetMainExecutable
// without being given the address of a function in the main executable).
std::string
GetExecutablePath(
    const char* Argv0,
    void* MainAddr)
{
    return llvm::sys::fs::getMainExecutable(Argv0, MainAddr);
}

//------------------------------------------------

namespace clang {
namespace mrdox {

} // mrdox
} // clang

//------------------------------------------------

int
main(int argc, const char** argv)
{
    // VFALCO GARBAGE
    force_xml_generator_linkage();

    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
    std::error_code OK;

    clang::mrdox::Config cfg;
    if(llvm::Error err = setupConfig(cfg, argc, argv))
    {
        llvm::errs() << err << "\n";
        return EXIT_FAILURE;
    }

    clang::mrdox::Corpus corpus;

    // Extract the AST first
    if(llvm::Error err = doMapping(corpus, cfg))
    {
        llvm::errs() <<
            toString(std::move(err)) << "\n";
        return EXIT_FAILURE;
    }

    // Build the internal representation of
    // the C++ declarations to be documented.
    if(llvm::Error err = buildIndex(corpus, *cfg.Executor->getToolResults(), cfg))
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
