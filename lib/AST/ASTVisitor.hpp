//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_TOOL_AST_ASTVISITOR_HPP
#define MRDOX_TOOL_AST_ASTVISITOR_HPP

#include "Tool/ConfigImpl.hpp"
#include "Tool/Diagnostics.hpp"
#include "Tool/ExecutionContext.hpp"
#include <clang/Sema/SemaConsumer.h>

namespace clang {
namespace mrdox {

class ASTVisitorConsumer
    : public SemaConsumer
{
    const ConfigImpl& config_;
    ExecutionContext& ex_;
    CompilerInstance& compiler_;

    Sema* sema_ = nullptr;

    //------------------------------------------------

    void InitializeSema(Sema& S) override;
    void ForgetSema() override;
    
    /** AST traversal entry point
    */
    void HandleTranslationUnit(ASTContext& Context) override;

    bool shouldSkipFunctionBody(Decl* D) override;
    bool HandleTopLevelDecl(DeclGroupRef D) override;
    ASTMutationListener* GetASTMutationListener() override;

    void HandleCXXStaticMemberVarInstantiation(VarDecl* D) override;
    void HandleCXXImplicitFunctionInstantiation(FunctionDecl* D) override;

    void HandleInlineFunctionDefinition(FunctionDecl* D) override;
    void HandleTagDeclDefinition(TagDecl* D) override;
    void HandleTagDeclRequiredDefinition(const TagDecl* D) override;
    void HandleInterestingDecl(DeclGroupRef D) override;
    void CompleteTentativeDefinition(VarDecl* D) override;
    void CompleteExternalDeclaration(VarDecl* D) override;
    void AssignInheritanceModel(CXXRecordDecl* D) override;
    void HandleVTable(CXXRecordDecl* D) override;
    void HandleImplicitImportDecl(ImportDecl* D) override;
    void HandleTopLevelDeclInObjCContainer(DeclGroupRef D) override;

public:
    ASTVisitorConsumer(
        const ConfigImpl& config,
        tooling::ExecutionContext& ex,
        CompilerInstance& compiler) noexcept;

};

} // mrdox
} // clang

#endif
