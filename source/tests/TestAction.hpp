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
#include <clang/Tooling/Execution.h>

namespace clang {
namespace mrdox {

//------------------------------------------------

/** A Visitor which stores tool results in a local map
*/
struct TestVisitor : public BasicVisitor
{
    explicit
    TestVisitor(
        Config const& cfg) noexcept
        : BasicVisitor(cfg)
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

    tooling::InMemoryToolResults results_;
};

//------------------------------------------------

struct TestAction
    : public clang::ASTFrontendAction
{
    explicit
    TestAction(
        Config const& cfg) noexcept
        : cfg_(cfg)
    {
    }

    std::unique_ptr<clang::ASTConsumer>
    CreateASTConsumer(
        clang::CompilerInstance& Compiler,
        llvm::StringRef InFile) override
    {
        return std::make_unique<TestVisitor>(cfg_);
    }

private:
    Config const& cfg_;
};

//------------------------------------------------

struct TestFactory
    : public tooling::FrontendActionFactory
{
    explicit
    TestFactory(
        Config const& cfg) noexcept
        : cfg_(cfg)
    {
    }

    std::unique_ptr<FrontendAction>
    create() override
    {
        return std::make_unique<TestAction>(cfg_);
    }

private:
    Config const& cfg_;
};

} // mrdox
} // clang

#endif
