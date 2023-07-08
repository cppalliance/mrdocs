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

#ifndef MRDOX_TOOL_TOOL_TOOLEXECUTOR_HPP
#define MRDOX_TOOL_TOOL_TOOLEXECUTOR_HPP

#include "ExecutionContext.hpp"
#include <mrdox/Config.hpp>
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/Execution.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/Mutex.h>
#include <optional>

namespace clang {
namespace mrdox {

/** A custom tool executor to run a front-end action.

    This tool executor permits running one action
    on all the TUs in a compilation database, using
    the settings specified in the @ref Config.

    In addition, the executor uses a custom
    execution context which the visitor retrieves
    from the regular execution context by using
    a downcast.
*/
class ToolExecutor : public tooling::ToolExecutor
{
public:
    using tooling::ToolExecutor::execute;

    ToolExecutor(
        report::Level reportLevel,
        Config const& config,
        tooling::CompilationDatabase const& Compilations,
        std::shared_ptr<PCHContainerOperations> PCHContainerOps =
            std::make_shared<PCHContainerOperations>());

    constexpr report::Level getReportLevel() const noexcept
    {
        return reportLevel_;
    }

    StringRef
    getExecutorName() const override
    {
        return "mrdox::ToolExecutor";
    }

    llvm::Error
    execute(llvm::ArrayRef<std::pair<std::unique_ptr<
        tooling::FrontendActionFactory>,
        tooling::ArgumentsAdjuster>> Actions) override;

    tooling::ExecutionContext*
    getExecutionContext() override
    {
        return &Context;
    };

    tooling::ToolResults*
    getToolResults() override
    {
        return Results.get();
    }

    void
    mapVirtualFile(
        StringRef FilePath,
        StringRef Content) override
    {
        OverlayFiles[FilePath] = std::string(Content);
    }

private:
    report::Level reportLevel_;
    Config const& config_;
    tooling::CompilationDatabase const& Compilations;
    std::unique_ptr<tooling::ToolResults> Results;
    llvm::StringMap<std::string> OverlayFiles;
    ExecutionContext Context;
};

} // mrdox
} // clang

#endif
