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

#ifndef MRDOX_TOOL_AST_EXECUTION_HPP
#define MRDOX_TOOL_AST_EXECUTION_HPP

#include <mrdox/Config.hpp>
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/Execution.h>
#include <llvm/ADT/SmallString.h>
#include <optional>

namespace clang {
namespace mrdox {

/// Executes given frontend actions on all files/TUs in the compilation
/// database.
class ToolExecutor : public tooling::ToolExecutor
{
public:
    using tooling::ToolExecutor::execute;

    ToolExecutor(
        Config const& config,
        tooling::CompilationDatabase const& Compilations,
        std::shared_ptr<PCHContainerOperations> PCHContainerOps =
            std::make_shared<PCHContainerOperations>());

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
    Config const& config_;
    tooling::CompilationDatabase const& Compilations;
    std::unique_ptr<tooling::ToolResults> Results;
    tooling::ExecutionContext Context;
    llvm::StringMap<std::string> OverlayFiles;
};

} // mrdox
} // clang

#endif
