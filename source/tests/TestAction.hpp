//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_TEST_TEST_VISITOR_HPP
#define MRDOX_TEST_TEST_VISITOR_HPP

#include <mrdox/BasicVisitor.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/Reporter.hpp>
#include <clang/Tooling/Execution.h>

namespace clang {
namespace mrdox {

//------------------------------------------------

/** A Visitor which stores tool results in a local map
*/
struct TestVisitor : public BasicVisitor
{
    TestVisitor(
        tooling::InMemoryToolResults& results,
        Config const& cfg,
        Reporter& R) noexcept
        : BasicVisitor(cfg)
        , results_(results)
        , R_(R)
    {
    }

private:
    void
    reportResult(
        llvm::StringRef Key,
        llvm::StringRef Value) override
    {
        results_.addResult(Key, Value);
    }

    tooling::InMemoryToolResults& results_;
    Reporter& R_;
};

//------------------------------------------------

/** Frontend action to run the test visitor.
*/
struct TestAction
    : public clang::ASTFrontendAction
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
        return std::make_unique<TestVisitor>(results_, cfg_, R_);
    }

    void EndSourceFileAction();

private:
    Config const& cfg_;
    tooling::InMemoryToolResults results_;
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
