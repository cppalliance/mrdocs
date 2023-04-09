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

//
// This file implements the main entry point for the clang-doc tool. It runs
// the clang-doc mapper on a given set of source code files using a
// FrontendActionFactory.
//

#include "ClangDoc.h"
#include <mrdox/Visitor.hpp>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>

namespace clang {
namespace mrdox {

namespace {

//------------------------------------------------

struct Action
    : public clang::ASTFrontendAction
{
    Action(
        tooling::ExecutionContext& exc,
        Config const& config) noexcept
        : exc_(exc)
        , config_(config)
    {
    }

    std::unique_ptr<clang::ASTConsumer>
    CreateASTConsumer(
        clang::CompilerInstance& Compiler,
        llvm::StringRef InFile) override
    {
        return std::make_unique<Visitor>(exc_, config_);
    }

private:
    tooling::ExecutionContext& exc_;
    Config const& config_;
};

//------------------------------------------------

struct Factory
    : public tooling::FrontendActionFactory
{
    Factory(
        tooling::ExecutionContext& exc,
        Config const& config) noexcept
        : exc_(exc)
        , config_(config)
    {
    }

    std::unique_ptr<FrontendAction>
    create() override
    {
        return std::make_unique<Action>(exc_, config_);
    }

private:
    tooling::ExecutionContext& exc_;
    Config const& config_;
};

} // (anon)

//------------------------------------------------

std::unique_ptr<tooling::FrontendActionFactory>
makeToolFactory(
    tooling::ExecutionContext& exc,
    Config const& config)
{
    return std::make_unique<Factory>(exc, config);
}

} // mrdox
} // clang
