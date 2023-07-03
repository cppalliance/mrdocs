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

#include "ToolExecutor.hpp"
#include <mrdox/Support/ThreadPool.hpp>
#include <clang/Tooling/ToolExecutorPluginRegistry.h>
#include <llvm/Support/Regex.h>
#include <llvm/Support/ThreadPool.h>
#include <llvm/Support/Threading.h>
#include <llvm/Support/VirtualFileSystem.h>

namespace clang {
namespace mrdox {

namespace {

llvm::Error
make_string_error(
    const llvm::Twine& Message)
{
    return llvm::make_error<llvm::StringError>(
        Message, llvm::inconvertibleErrorCode());
}

tooling::ArgumentsAdjuster
getDefaultArgumentsAdjusters()
{
    return tooling::combineAdjusters(
        tooling::getClangStripOutputAdjuster(),
        tooling::combineAdjusters(
            tooling::getClangSyntaxOnlyAdjuster(),
            tooling::getClangStripDependencyFileAdjuster()));
}

//------------------------------------------------

class ThreadSafeToolResults : public tooling::ToolResults
{
public:
    void addResult(StringRef Key, StringRef Value) override
    {
        std::unique_lock<std::mutex> LockGuard(Mutex);
        Results.addResult(Key, Value);
    }

    std::vector<std::pair<llvm::StringRef, llvm::StringRef>>
        AllKVResults() override
    {
        return Results.AllKVResults();
    }

    void forEachResult(llvm::function_ref<
        void(StringRef Key, StringRef Value)> Callback) override
    {
        Results.forEachResult(Callback);
    }

private:
    tooling::InMemoryToolResults Results;
    std::mutex Mutex;
};

//------------------------------------------------

} // (anon)

ToolExecutor::
ToolExecutor(
    Config const& config,
    tooling::CompilationDatabase const& Compilations,
    std::shared_ptr<PCHContainerOperations> PCHContainerOps)
    : config_(config)
    , Compilations(Compilations)
    , Results(new ThreadSafeToolResults)
    , Context(Results.get())
{
}

llvm::Error
ToolExecutor::
execute(
    llvm::ArrayRef<std::pair<
        std::unique_ptr<tooling::FrontendActionFactory>,
        tooling::ArgumentsAdjuster>> Actions)
{
    if (Actions.empty())
        return make_string_error("No action to execute.");

    if (Actions.size() != 1)
        return make_string_error(
            "Only support executing exactly 1 action at this point.");

    std::string ErrorMsg;
    std::mutex TUMutex;
    auto AppendError = [&](llvm::Twine Err)
    {
        std::unique_lock<std::mutex> LockGuard(TUMutex);
        ErrorMsg += Err.str();
    };

    auto Log = [&](llvm::Twine Msg)
    {
        std::unique_lock<std::mutex> LockGuard(TUMutex);
        llvm::errs() << Msg.str() << "\n";
    };

    auto Files = Compilations.getAllFiles();

    // Add a counter to track the progress.
    const std::string TotalNumStr = std::to_string(Files.size());
    unsigned Counter = 0;
    auto Count = [&]()
    {
        std::unique_lock<std::mutex> LockGuard(TUMutex);
        return ++Counter;
    };

    auto& Action = Actions.front();

    TaskGroup taskGroup(config_.threadPool());

    for (std::string File : Files)
    {
        taskGroup.async(
        [&, Path = std::move(File)]()
        {
            if(config_.verboseOutput)
                Log("[" + std::to_string(Count()) + "/" + TotalNumStr + "] Processing file " + Path);

            // Each thread gets an independent copy of a VFS to allow different
            // concurrent working directories.
            IntrusiveRefCntPtr<llvm::vfs::FileSystem> FS =
                llvm::vfs::createPhysicalFileSystem();

            tooling::ClangTool Tool( Compilations, { Path },
                std::make_shared<PCHContainerOperations>(), FS);
            Tool.appendArgumentsAdjuster(Action.second);
            Tool.appendArgumentsAdjuster(getDefaultArgumentsAdjusters());

            for (const auto& FileAndContent : OverlayFiles)
                Tool.mapVirtualFile(FileAndContent.first(),
                    FileAndContent.second);

            if (Tool.run(Action.first.get()))
                AppendError(llvm::Twine("Failed to run action on ") + Path + "\n");
        });
    }

    auto errors = taskGroup.wait();

    if (!ErrorMsg.empty())
        return make_string_error(ErrorMsg);

    return llvm::Error::success();
}

} // mrdox
} // clang
