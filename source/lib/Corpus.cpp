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
#include "ClangDoc.h"
#include "Serialize.h"
#include <mrdox/Corpus.hpp>
#include <clang/Tooling/AllTUsExecution.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <llvm/Support/Mutex.h>
#include <llvm/Support/ThreadPool.h>

namespace clang {
namespace mrdox {

//------------------------------------------------
//
// Observers
//
//------------------------------------------------

Info const*
Corpus::
find(
    SymbolID const& id) const noexcept
{
    auto it = InfoMap.find(llvm::toStringRef(id));
    if(it != InfoMap.end())
        return it->second.get();
    return nullptr;
}

Info const&
Corpus::
at(
    SymbolID const& id) const noexcept
{
    auto it = InfoMap.find(llvm::toStringRef(id));
    assert(it != InfoMap.end());
    return *it->second.get();
}

//------------------------------------------------
//
// Implementation
//
//------------------------------------------------

void
Corpus::
insert(std::unique_ptr<Info> Ip)
{
    auto const& I = *Ip;

    // Store the Info in the result map
    {
        std::lock_guard<llvm::sys::Mutex> Guard(infoMutex);
        InfoMap[llvm::toStringRef(I.USR)] = std::move(Ip);
    }

    // Add a reference to this Info in the Index
    insertIntoIndex(I);

    // Visit children
}

// A function to add a reference to Info in Idx.
// Given an Info X with the following namespaces: [B,A]; a reference to X will
// be added in the children of a reference to B, which should be also a child of
// a reference to A, where A is a child of Idx.
//   Idx
//    |-- A
//        |--B
//           |--X
// If the references to the namespaces do not exist, they will be created. If
// the references already exist, the same one will be used.
void
Corpus::
insertIntoIndex(
    Info const& I)
{
    std::lock_guard<llvm::sys::Mutex> Guard(indexMutex);

    // Index pointer that will be moving through Idx until the first parent
    // namespace of Info (where the reference has to be inserted) is found.
    Index* pi = &Idx;
    // The Namespace vector includes the upper-most namespace at the end so the
    // loop will start from the end to find each of the namespaces.
    for (const auto& R : llvm::reverse(I.Namespace))
    {
        // Look for the current namespace in the children of the index pi is
        // pointing.
        auto It = llvm::find(pi->Children, R.USR);
        if (It != pi->Children.end())
        {
            // If it is found, just change pi to point the namespace reference found.
            pi = &*It;
        }
        else
        {
            // If it is not found a new reference is created
            pi->Children.emplace_back(R.USR, R.Name, R.RefType, R.Path);
            // pi is updated with the reference of the new namespace reference
            pi = &pi->Children.back();
        }
    }
    // Look for Info in the vector where it is supposed to be; it could already
    // exist if it is a parent namespace of an Info already passed to this
    // function.
    auto It = llvm::find(pi->Children, I.USR);
    if (It == pi->Children.end())
    {
        // If it is not in the vector it is inserted
        pi->Children.emplace_back(I.USR, I.extractName(), I.IT,
            I.Path);
    }
    else
    {
        // If it not in the vector we only check if Path and Name are not empty
        // because if the Info was included by a namespace it may not have those
        // values.
        if (It->Path.empty())
            It->Path = I.Path;
        if (It->Name.empty())
            It->Name = I.extractName();
    }

    allSymbols.emplace_back(I.USR);
}

//------------------------------------------------

void
Corpus::
reportResult(
    tooling::ExecutionContext& exc,
    Info const& I)
{
    std::string s = serialize(I);
    exc.reportResult(
        llvm::toStringRef(I.USR), std::move(s));
}

//------------------------------------------------

std::unique_ptr<Corpus>
Corpus::
build(
    tooling::ToolExecutor& ex,
    Config const& config,
    Reporter& R)
{
    auto up = std::unique_ptr<Corpus>(new Corpus);
    Corpus& corpus = *up;

    // Traverse the AST for all translation units
    // and emit serializd bitcode into tool results.
    // This operation happens ona thread pool.

    llvm::outs() << "Mapping declarations\n";
    llvm::Error err = ex.execute(
        makeToolFactory(*ex.getExecutionContext(), config, R),
        config.ArgAdjuster);
    if(err)
    {
        if(! config.IgnoreMappingFailures)
        {
            llvm::errs() <<
                "Mapping failure: " << toString(std::move(err)) << "\n";
            return nullptr;
        }

        llvm::errs() <<
            "Error mapping decls in files. "
            "MrDox will ignore these files and continue:\n" <<
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
            // One or more Info for the same symbol ID
            std::vector<std::unique_ptr<Info>> Infos;

            // Each Bitcode can have multiple Infos
            for (auto& Bitcode : Group.getValue())
            {
                llvm::BitstreamCursor Stream(Bitcode);
                ClangDocBitcodeReader Reader(Stream);
                auto ReadInfos = Reader.readBitcode();
                if(! ReadInfos)
                {
                    R.failed(ReadInfos.takeError());
                    GotFailure = true;
                    return;
                }
                std::move(
                    ReadInfos->begin(),
                    ReadInfos->end(),
                    std::back_inserter(Infos));
            }

            auto mergeResult = mergeInfos(Infos);
            if(!mergeResult)
            {
                // VFALCO What about GotFailure?
                R.failed("mergeInfos", mergeResult.takeError());
                return;
            }

            std::unique_ptr<Info> Ip(mergeResult.get().release());
            assert(Group.getKey() == llvm::toStringRef(Ip->USR));
            corpus.insert(std::move(Ip));
        });
    }

    Pool.wait();

    llvm::outs() <<
        "Collected " << corpus.InfoMap.size() << " symbols.\n";

    if(GotFailure)
    {
        R.failed("buildCorpus",
            llvm::createStringError(
                llvm::inconvertibleErrorCode(),
                "an error occurred"));
    }

    //
    // Finish up
    //

    // Sort allSymbols by fully qualified name
    {
        std::string temp[2];
        llvm::sort(
            corpus.allSymbols,
            [&](SymbolID const& id0,
                SymbolID const& id1) noexcept
            {
                auto s0 = corpus.at(id0).getFullyQualifiedName(temp[0]);
                auto s1 = corpus.at(id1).getFullyQualifiedName(temp[1]);
                int rv = s0.compare_insensitive(s1);
                if(rv < 0)
                    return true;
                if(rv > 0)
                    return false;
                return s0.compare(s1) < 0;
            });
    }

    return std::move(up);
}

} // mrdox
} // clang
