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
#include "Mapper.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"

namespace clang {
namespace mrdox {

namespace {

// VFALCO It looks like each created action needs
//        its own copy of the ClangDocContext?
//        Maybe because of concurrency.

//------------------------------------------------

struct action
    : public clang::ASTFrontendAction
{
    explicit
    action(
        ClangDocContext& CDCtx)
        : CDCtx(CDCtx)
    {
    }

    std::unique_ptr<clang::ASTConsumer>
    CreateASTConsumer(
        clang::CompilerInstance& Compiler,
        llvm::StringRef InFile) override
    {
        return std::make_unique<MapASTVisitor>(
            &Compiler.getASTContext(), CDCtx);
    }

private:
    ClangDocContext& CDCtx;
};

//------------------------------------------------

struct factory
    : public tooling::FrontendActionFactory
{
    explicit
    factory(
        ClangDocContext& CDCtx)
        : CDCtx(CDCtx)
    {
    }

    std::unique_ptr<FrontendAction>
    create() override
    {
        return std::make_unique<action>(CDCtx);
    }
        
    ClangDocContext& CDCtx;
};

} // (anon)

//------------------------------------------------

std::unique_ptr<
    clang::FrontendAction>
makeFrontendAction(
    ClangDocContext& CDCtx)
{
    return std::make_unique<action>(CDCtx);
}

std::unique_ptr<
    tooling::FrontendActionFactory>
newMapperActionFactory(
    ClangDocContext& CDCtx)
{
    return std::make_unique<factory>(CDCtx);
}

} // namespace mrdox
} // namespace clang
