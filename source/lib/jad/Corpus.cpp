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

namespace {

class ThreadSafeToolResults
    : public tooling::ToolResults
{
public:
    void
    addResult(
        llvm::StringRef Key,
        llvm::StringRef Value) override
    {
        std::unique_lock<std::mutex> LockGuard(Mutex);
        Results.addResult(Key, Value);
    }

    std::vector<std::pair<llvm::StringRef, llvm::StringRef>>
    AllKVResults() override
    {
        return Results.AllKVResults();
    }

    void
    forEachResult(
        llvm::function_ref<void(
            llvm::StringRef Key, llvm::StringRef Value)> Callback) override
    {
        Results.forEachResult(Callback);
    }

private:
    tooling::InMemoryToolResults Results;
    std::mutex Mutex;
};

} // (anon)

//------------------------------------------------

Corpus::
Corpus()
    : toolResults(std::make_unique<ThreadSafeToolResults>())
{
}

//------------------------------------------------

llvm::Error
doMapping(
    Corpus& corpus,
    Config const& cfg)
{
    //
    // Mapping phase
    //
    llvm::outs() << "Mapping declarations\n";
    auto Err = cfg.Executor->execute(
        newMapperActionFactory(corpus, cfg),
        cfg.ArgAdjuster);
    if(Err)
    {
        if(! cfg.IgnoreMappingFailures)
        {
            llvm::errs() <<
                "Mapping failure: " << Err << "\n";
            return Err;
        }

        llvm::errs() <<
            "Error mapping decls in files. mrdox will ignore "
            "these files and continue:\n" <<
            toString(std::move(Err)) << "\n";
    }
    return llvm::Error::success();
}

llvm::Error
buildIndex(
    Corpus& corpus,
    Config const& cfg)
{
/*  Collect all symbols. The result is a vector of
    one or more bitcodes for each symbol. These will
    be merged later.
*/
    llvm::outs() << "Collecting symbols\n";
    llvm::StringMap<std::vector<StringRef>> USRToBitcode;
    corpus.toolResults->forEachResult(
        [&](StringRef Key, StringRef Value)
        {
            auto R = USRToBitcode.try_emplace(Key, std::vector<StringRef>());
            R.first->second.emplace_back(Value);
        });

    // Collects all Infos according to their unique USR value. This map is added
    // to from the thread pool below and is protected by the USRToInfoMutex.
    llvm::sys::Mutex USRToInfoMutex;

    // First reducing phase (reduce all decls into one info per decl).
    llvm::outs() << "Reducing " << USRToBitcode.size() << " declarations\n";
    std::atomic<bool> GotFailure;
    GotFailure = false;
    llvm::sys::Mutex IndexMutex;
    // ExecutorConcurrency is a flag exposed by AllTUsExecution.h
    llvm::ThreadPool Pool(llvm::hardware_concurrency(tooling::ExecutorConcurrency));
    for (auto& Group : USRToBitcode)
    {
        Pool.async([&]()
        {
            std::vector<std::unique_ptr<mrdox::Info>> Infos;

            for (auto& Bitcode : Group.getValue())
            {
                llvm::BitstreamCursor Stream(Bitcode);
                mrdox::ClangDocBitcodeReader Reader(Stream);
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

            auto Reduced = mrdox::mergeInfos(Infos);
            if (!Reduced)
            {
                // VFALCO What about GotFailure?
                llvm::errs() << llvm::toString(Reduced.takeError());
                return;
            }

            // Add a reference to this Info in the Index
            {
                std::lock_guard<llvm::sys::Mutex> Guard(IndexMutex);
                clang::mrdox::Generator::addInfoToIndex(corpus.Idx, Reduced.get().get());
            }

            // Save in the result map (needs a lock due to threaded access).
            {
                std::lock_guard<llvm::sys::Mutex> Guard(USRToInfoMutex);
                corpus.USRToInfo[Group.getKey()] = std::move(Reduced.get());
            }
        });
    }

    Pool.wait();

    if (GotFailure)
        return llvm::createStringError(
            llvm::inconvertibleErrorCode(),
            "an error occurred");

    return llvm::Error::success();
}

} // mrdox
} // clang
