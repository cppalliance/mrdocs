//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_TEST_TEST_ACTION_HPP
#define MRDOX_TEST_TEST_ACTION_HPP

#include <mrdox/Corpus.hpp>
#include <mrdox/Visitor.hpp>
#include <mrdox/Errors.hpp>
#include <clang/Tooling/Execution.h>

namespace clang {
namespace mrdox {

/** Frontend test action to run the visitor.
*/
struct TestAction : clang::ASTFrontendAction
{
    TestAction(
        Config const& cfg,
        Reporter& R) noexcept
        : cfg_(cfg)
        , R_(R)
    {
    }

private:
    std::unique_ptr<clang::ASTConsumer>
    CreateASTConsumer(
        clang::CompilerInstance& Compiler,
        llvm::StringRef InFile) override
    {
        return std::make_unique<Visitor>(corpus_, cfg_);
    }

    void EndSourceFileAction();

private:
    Config const& cfg_;
    Corpus corpus_;
    Reporter& R_;
};

//------------------------------------------------

/** Factory boilerplate for creating test actions.
*/
struct TestFactory
    : public tooling::FrontendActionFactory
{
    TestFactory(
        Config const& cfg,
        Reporter& R) noexcept
        : cfg_(cfg)
        , R_(R)
    {
    }

    std::unique_ptr<FrontendAction>
    create() override
    {
        return std::make_unique<TestAction>(cfg_, R_);
    }

private:
    Config const& cfg_;
    Reporter& R_;
};

} // mrdox
} // clang

#endif
