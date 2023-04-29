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

#ifndef MRDOX_API_AST_ASTVISITOR_HPP
#define MRDOX_API_AST_ASTVISITOR_HPP

#include <mrdox/Platform.hpp>
#include "ConfigImpl.hpp"
#include <mrdox/Reporter.hpp>
#include <clang/AST/Mangle.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Tooling/Execution.h>
#include <unordered_map>

namespace clang {
namespace mrdox {

/** Convert AST to our metadata and serialize to bitcode.

    An instance of this object visits the AST
    for exactly one translation unit. The AST is
    converted into our metadata, and this metadata
    is then serialized into bitcode. The resulting
    bitcode is inserted into the tool results,
    keyed by ID. Each ID can have multiple
    serialized bitcodes, as the same declaration
    in a particular include file can be seen by
    more than one translation unit.
*/
class ASTVisitor
    : public RecursiveASTVisitor<ASTVisitor>
    , public ASTConsumer
{
    struct FileFilter
    {
        llvm::SmallString<0> prefix;
        bool include = true;
    };

    tooling::ExecutionContext& ex_;
    ConfigImpl const& config_;
    Reporter& R_;
    std::unordered_map<
        clang::SourceLocation::UIntTy,
        FileFilter> fileFilter_;
    std::unique_ptr<MangleContext> mc_;

public:
    ASTVisitor(
        tooling::ExecutionContext& ex,
        ConfigImpl const& config,
        Reporter& R) noexcept
        : ex_(ex)
        , config_(config)
        , R_(R)
    {
    }

//private:
    void HandleTranslationUnit(ASTContext& Context) override;

    bool WalkUpFromNamespaceDecl(NamespaceDecl* D);
    bool WalkUpFromCXXRecordDecl(CXXRecordDecl* D);
    bool WalkUpFromCXXMethodDecl(CXXMethodDecl* D);
    bool WalkUpFromFriendDecl(FriendDecl* D);
    //bool WalkUpFromUsingDecl(UsingDecl* D);
    bool WalkUpFromUsingShadowDecl(UsingShadowDecl* D);
    bool WalkUpFromFunctionDecl(FunctionDecl* D);
    bool WalkUpFromTypeAliasDecl(TypeAliasDecl* D);
    bool WalkUpFromTypedefDecl(TypedefDecl* D);
    bool WalkUpFromEnumDecl(EnumDecl* D);
    bool WalkUpFromVarDecl(VarDecl* D);

private:
    template <typename T>
    bool mapDecl(T const* D);

    int
    getLine(
        NamedDecl const* D,
        ASTContext const& Context) const;

    llvm::SmallString<128>
    getFile(
        NamedDecl const* D, 
        ASTContext const& Context,
        StringRef RootDir,
        bool& IsFileInRootDir) const;
};

} // mrdox
} // clang

#endif
