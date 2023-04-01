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

#include "BitcodeReader.h"
#include "BitcodeWriter.h"
#include "ClangDoc.h"
#include "Generators.h"
#include "Representation.h"
#include "clang/AST/AST.h"
#include "clang/AST/Decl.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchersInternal.h"
#include "clang/Driver/Options.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/AllTUsExecution.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Execution.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Mutex.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/ThreadPool.h"
#include "llvm/Support/raw_ostream.h"
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



//------------------------------------------------

int
main(int argc, const char** argv)
{
    // VFALCO GARBAGE
    force_xml_generator_linkage();

    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
    std::error_code OK;

    clang::mrdox::ClangDocContext CDCtx;
    if(llvm::Error err = setupContext(CDCtx, argc, argv))
    {
        llvm::errs() << err << "\n";
        return EXIT_FAILURE;
    }

    // Mapping phase
    if(llvm::Error err = executeMapping(CDCtx))
    {
        llvm::errs() <<
            toString(std::move(err)) << "\n";
        return EXIT_FAILURE;
    }

    //--------------------------------------------

    // Collect values into output by key.
    // In ToolResults, the Key is the hashed USR and the value is the
    // bitcode-encoded representation of the Info object.
    llvm::outs() << "Collecting infos...\n";
    llvm::StringMap<std::vector<StringRef>> USRToBitcode;
    CDCtx.Executor->getToolResults()->forEachResult(
        [&](StringRef Key, StringRef Value)
        {
            auto R = USRToBitcode.try_emplace(Key, std::vector<StringRef>());
            R.first->second.emplace_back(Value);
        });

    // Collects all Infos according to their unique USR value. This map is added
    // to from the thread pool below and is protected by the USRToInfoMutex.
    llvm::sys::Mutex USRToInfoMutex;
    llvm::StringMap<std::unique_ptr<mrdox::Info>> USRToInfo;

    // First reducing phase (reduce all decls into one info per decl).
    llvm::outs() << "Reducing " << USRToBitcode.size() << " infos...\n";
    std::atomic<bool> Error;
    Error = false;
    llvm::sys::Mutex IndexMutex;
    // ExecutorConcurrency is a flag exposed by AllTUsExecution.h
    llvm::ThreadPool Pool(llvm::hardware_concurrency(ExecutorConcurrency));
    //llvm::ThreadPool Pool(llvm::hardware_concurrency(1));
    for (auto& Group : USRToBitcode) {
        Pool.async([&]() {
            std::vector<std::unique_ptr<mrdox::Info>> Infos;

            for (auto& Bitcode : Group.getValue()) {
                llvm::BitstreamCursor Stream(Bitcode);
                mrdox::ClangDocBitcodeReader Reader(Stream);
                auto ReadInfos = Reader.readBitcode();
                if (!ReadInfos) {
                    llvm::errs() << toString(ReadInfos.takeError()) << "\n";
                    Error = true;
                    return;
                }
                std::move(ReadInfos->begin(), ReadInfos->end(),
                    std::back_inserter(Infos));
            }

            auto Reduced = mrdox::mergeInfos(Infos);
            if (!Reduced) {
                llvm::errs() << llvm::toString(Reduced.takeError());
                return;
            }

            // Add a reference to this Info in the Index
            {
                std::lock_guard<llvm::sys::Mutex> Guard(IndexMutex);
                clang::mrdox::Generator::addInfoToIndex(CDCtx.Idx, Reduced.get().get());
            }

            // Save in the result map (needs a lock due to threaded access).
            {
                std::lock_guard<llvm::sys::Mutex> Guard(USRToInfoMutex);
                USRToInfo[Group.getKey()] = std::move(Reduced.get());
            }
            });
    }

    Pool.wait();

    if (Error)
        return 1;

    // Ensure the root output directory exists.
    if (std::error_code Err = llvm::sys::fs::create_directories(CDCtx.OutDirectory);
        Err != std::error_code()) {
        llvm::errs() << "Failed to create directory '" << CDCtx.OutDirectory << "'\n";
        return 1;
    }

    // Run the generator.
    llvm::outs() << "Generating docs...\n";
    if (auto Err =
        CDCtx.G->generateDocs(CDCtx.OutDirectory, std::move(USRToInfo), CDCtx)) {
        llvm::errs() << toString(std::move(Err)) << "\n";
        return 1;
    }

    //
    // Generate assets
    //
    {
        llvm::outs() << "Generating assets for docs...\n";
        auto Err = CDCtx.G->createResources(CDCtx);
        if (Err) {
            llvm::errs() << toString(std::move(Err)) << "\n";
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
