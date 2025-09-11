//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_AST_ASTACTION_HPP
#define MRDOCS_LIB_AST_ASTACTION_HPP

#include "lib/ConfigImpl.hpp"
#include "lib/Support/ExecutionContext.hpp"
#include <clang/Tooling/Tooling.h>
#include <mrdocs/Platform.hpp>
#include "MissingSymbolSink.hpp"


namespace clang {
namespace mrdocs {

/** The clang frontend action for visiting the AST

    This is the MrDocs Clang Frontend Action used by the Clang tooling
    to extract information from the AST.

    This is an AST consumer-based frontend action
    (`clang::ASTFrontendAction`) that 1) can create a
    `clang::ASTConsumer` that uses an `ASTVisitor` to traverse
    the AST and extract information and 2) parses the AST
    with this consumer.

    By overriding these methods, Clang will invoke the
    @ref ASTVisitor for each translation unit.
*/
class ASTAction
    : public clang::ASTFrontendAction
{
    ExecutionContext& ex_;
    ConfigImpl const& config_;
    MissingSymbolSink* missingSink_ = nullptr;

public:
    ASTAction(
        ExecutionContext& ex,
        ConfigImpl const& config) noexcept
        : ex_(ex)
        , config_(config)
    {
    }

    /** Execute the action

        This is called by the tooling infrastructure to execute
        the action for each translation unit.

        The function will set options on the `CompilerInstance`
        and parse the AST with the consumer that will have have
        been previously created with @ref CreateASTConsumer.

        This `clang::ASTConsumer` then creates an
        @ref ASTVisitor that will convert the AST into a set
        of MrDocs Info objects.
     */
    void
    ExecuteAction() override;

    /** Create the object that will traverse the AST

        This is called by the tooling infrastructure to create
        a `clang::ASTConsumer` for each translation unit.

        This consumer takes the TU and creates an
        @ref ASTVisitor that will convert the AST into a
        set of MrDocs Info types.

        The main function of the ASTVisitorConsumer is
        the `HandleTranslationUnit` function, which is called
        to traverse the AST with the @ref ASTVisitor.
     */
    std::unique_ptr<clang::ASTConsumer>
    CreateASTConsumer(
        clang::CompilerInstance& Compiler,
        llvm::StringRef InFile) override;

    void
    setMissingSymbolSink(MissingSymbolSink& sink) noexcept
    {
        missingSink_ = &sink;
    }
};

} // mrdocs
} // clang

#endif
