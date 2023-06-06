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

//===- lib/Tooling/AllTUsExecution.cpp - Execute actions on all TUs. ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Execution.hpp"
#include <clang/Tooling/ToolExecutorPluginRegistry.h>
#include <llvm/Support/Regex.h>
#include <llvm/Support/ThreadPool.h>
#include <llvm/Support/Threading.h>
#include <llvm/Support/VirtualFileSystem.h>

namespace clang {
namespace mrdox {

const char* Executor::ExecutorName = "MrDoxExecutor";

#if 0
llvm::cl::opt<std::string>
Filter("filter",
    llvm::cl::desc("Only process files that match this filter. "
        "This flag only applies to all-TUs."),
    llvm::cl::init(".*"));

llvm::cl::opt<unsigned> ExecutorConcurrency(
    "execute-concurrency",
    llvm::cl::desc("The number of threads used to process all files in "
        "parallel. Set to 0 for hardware concurrency. "
        "This flag only applies to all-TUs."),
    llvm::cl::init(0));
#endif

namespace {

llvm::Error
make_string_error(
    const llvm::Twine& Message)
{
    return llvm::make_error<llvm::StringError>(Message, llvm::inconvertibleErrorCode());
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

} // (anon)

Executor::
Executor(
    tooling::CompilationDatabase const& Compilations,
    llvm::StringRef workingDir,
    unsigned ThreadCount,
    std::shared_ptr<PCHContainerOperations> PCHContainerOps)
    : Compilations(Compilations)
    , workingDir_(workingDir)
    , Results(new ThreadSafeToolResults)
    , Context(Results.get())
    , ThreadCount(ThreadCount)
{
}

Executor::
Executor(
    tooling::CommonOptionsParser Options,
    llvm::StringRef workingDir,
    unsigned ThreadCount,
    std::shared_ptr<PCHContainerOperations> PCHContainerOps)
    : OptionsParser(std::move(Options))
    , Compilations(OptionsParser->getCompilations())
    , workingDir_(workingDir)
    , Results(new ThreadSafeToolResults), Context(Results.get())
    , ThreadCount(ThreadCount)
{
}

llvm::Error
Executor::
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

    std::vector<std::string> Files;
#if 0
    llvm::Regex RegexFilter(Filter);
    for (const auto& File : Compilations.getAllFiles())
    {
        if (RegexFilter.match(File))
            Files.push_back(File);
    }
#else
    Files = Compilations.getAllFiles();
#endif
    // Add a counter to track the progress.
    const std::string TotalNumStr = std::to_string(Files.size());
    unsigned Counter = 0;
    auto Count = [&]()
    {
        std::unique_lock<std::mutex> LockGuard(TUMutex);
        return ++Counter;
    };

    auto& Action = Actions.front();

    {
        llvm::ThreadPool Pool(llvm::hardware_concurrency(ThreadCount));
        for (std::string File : Files) {
            Pool.async(
                [&](std::string Path) {
                    Log("[" + std::to_string(Count()) + "/" + TotalNumStr +
                        "] Processing file " + Path);
                    // Each thread gets an independent copy of a VFS to allow different
                    // concurrent working directories.
                    IntrusiveRefCntPtr<llvm::vfs::FileSystem> FS =
                        llvm::vfs::createPhysicalFileSystem();
                    FS->setCurrentWorkingDirectory(workingDir_);
                    tooling::ClangTool Tool(Compilations, { Path },
                        std::make_shared<PCHContainerOperations>(), FS);
                    Tool.appendArgumentsAdjuster(Action.second);
                    Tool.appendArgumentsAdjuster(getDefaultArgumentsAdjusters());
                    for (const auto& FileAndContent : OverlayFiles)
                        Tool.mapVirtualFile(FileAndContent.first(),
                            FileAndContent.second);
                    if (Tool.run(Action.first.get()))
                        AppendError(llvm::Twine("Failed to run action on ") + Path +
                            "\n");
                },
                File);
        }
        // Make sure all tasks have finished before resetting the working directory.
        Pool.wait();
    }

    if (!ErrorMsg.empty())
        return make_string_error(ErrorMsg);

    return llvm::Error::success();
}

#if 0
class ExecutorPlugin : public tooling::ToolExecutorPlugin {
public:
    llvm::Expected<std::unique_ptr<tooling::ToolExecutor>>
    create(tooling::CommonOptionsParser& OptionsParser) override
    {
        if (OptionsParser.getSourcePathList().empty())
            return make_string_error(
                "[ExecutorPlugin] Please provide a directory/file path in "
                "the compilation database.");
        return std::make_unique<Executor>(std::move(OptionsParser),
            ExecutorConcurrency);
    }
};

static tooling::ToolExecutorPluginRegistry::Add<ExecutorPlugin> X(
    "all-TUs",
    "Runs FrontendActions on all TUs in the compilation database. "
    "Tool results are stored in memory.");

// This anchor is used to force the linker to link in the generated object file
// and thus register the plugin.
volatile int ExecutorPluginAnchor = 0;
#endif

} // mrdox
} // clang
