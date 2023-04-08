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

#include "BitcodeReader.h"
#include "BitcodeWriter.h"
#include "Generators.h"
#include "ClangDoc.h"
#include <mrdox/Corpus.hpp>
#include <clang/Tooling/AllTUsExecution.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <llvm/Support/Mutex.h>
#include <llvm/Support/ThreadPool.h>

namespace clang {
namespace mrdox {

//------------------------------------------------

std::unique_ptr<Corpus>
buildCorpus(
    tooling::ToolExecutor& ex,
    Config const& cfg,
    Reporter& R)
{
    Corpus corpus;

    // Traverse the AST for all translation
    // units and emit serializd bitcode into
    // tool results. This operation happens
    // on a thread pool.
    //
    llvm::outs() << "Mapping declarations\n";
    llvm::Error err = ex.execute(
        makeToolFactory(*ex.getExecutionContext(), cfg),
        cfg.ArgAdjuster);
    if(err)
    {
        if(! cfg.IgnoreMappingFailures)
        {
            llvm::errs() <<
                "Mapping failure: " << toString(std::move(err)) << "\n";
            return nullptr;
        }

        llvm::errs() <<
            "Error mapping decls in files. mrdox will ignore "
            "these files and continue:\n" <<
            toString(std::move(err)) << "\n";
    }

    // Collect the symbols. Each symbol will have
    // a vector of one or more bitcodes. These will
    // be merged later.
    llvm::outs() << "Collecting symbols\n";
    llvm::StringMap<std::vector<StringRef>> USRToBitcode;
    ex.getToolResults()->forEachResult(
        [&](StringRef Key, StringRef Value)
        {
            auto R = USRToBitcode.try_emplace(Key, std::vector<StringRef>());
            R.first->second.emplace_back(Value);
        });

    // Collects all Infos according to their unique
    // USR value. This map is added to from the thread
    // pool below and is protected by this mutex.
    llvm::sys::Mutex USRToInfoMutex;

    // First reducing phase (reduce all decls into one info per decl).
    llvm::outs() << "Reducing " << USRToBitcode.size() << " declarations\n";
    std::atomic<bool> GotFailure;
    GotFailure = false;
    llvm::sys::Mutex IndexMutex;
    // VFALCO Should this concurrency be a command line option?
    llvm::ThreadPool Pool(llvm::hardware_concurrency(tooling::ExecutorConcurrency));
    for (auto& Group : USRToBitcode)
    {
        Pool.async([&]()
        {
            std::vector<std::unique_ptr<Info>> Infos;

            for (auto& Bitcode : Group.getValue())
            {
                llvm::BitstreamCursor Stream(Bitcode);
                ClangDocBitcodeReader Reader(Stream);
                auto ReadInfos = Reader.readBitcode();
                if (!ReadInfos)
                {
                    llvm::errs() << toString(ReadInfos.takeError()) << "\n";
                    GotFailure = true;
                    return;
                }
                std::move(
                    ReadInfos->begin(),
                    ReadInfos->end(),
                    std::back_inserter(Infos));
            }

            auto Reduced = mergeInfos(Infos);
            if (!Reduced)
            {
                // VFALCO What about GotFailure?
                llvm::errs() << llvm::toString(Reduced.takeError());
                return;
            }

            // Add a reference to this Info in the Index
            {
                std::lock_guard<llvm::sys::Mutex> Guard(IndexMutex);
                Generator::addInfoToIndex(corpus.Idx, Reduced.get().get());
            }

            // Save in the result map (needs a lock due to threaded access).
            {
                std::lock_guard<llvm::sys::Mutex> Guard(USRToInfoMutex);
                corpus.USRToInfo[Group.getKey()] = std::move(Reduced.get());
            }
        });
    }

    Pool.wait();

    if(GotFailure)
    {
        R.failed("buildCorpus",
            llvm::createStringError(
                llvm::inconvertibleErrorCode(),
                "an error occurred"));
    }

    return std::make_unique<Corpus>(std::move(corpus));
}

} // mrdox
} // clang
