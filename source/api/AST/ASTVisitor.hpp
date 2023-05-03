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
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Tooling/Execution.h>
#include <unordered_map>

namespace clang {
namespace mrdox {

struct SerializeResult;

/** Convert AST to our metadata and serialize to bitcode.

    An instance of this object visits the AST
    for exactly one translation unit. The AST is
    extracted and converted into our metadata, and
    this metadata is then serialized into bitcode.
    The resulting bitcode is inserted into the tool
    results, keyed by ID. Each ID can have multiple
    serialized bitcodes, as the same declaration
    in a particular include file can be seen by
    more than one translation unit.
*/
class ASTVisitor
    : public RecursiveASTVisitor<ASTVisitor>
    , public ASTConsumer
{
public:
    struct FileFilter
    {
        llvm::SmallString<0> prefix;
        bool include = true;
    };

    tooling::ExecutionContext& ex_;
    ConfigImpl const& config_;
    Reporter& R_;

    llvm::SmallString<512> File;
    int LineNumber;
    bool PublicOnly;
    bool IsFileInRootDir;

    ASTContext* astContext_;
    clang::SourceManager const* sourceManager_;
    std::unordered_map<
        clang::SourceLocation::UIntTy,
        FileFilter> fileFilter_;

public:
    ASTVisitor(
        tooling::ExecutionContext& ex,
        ConfigImpl const& config,
        Reporter& R) noexcept;

private:
    SerializeResult build(NamespaceDecl*   D);
    SerializeResult build(CXXRecordDecl*   D);
    SerializeResult build(CXXMethodDecl*   D);
    SerializeResult build(FriendDecl*      D);
    SerializeResult build(UsingDecl*       D);
    SerializeResult build(UsingShadowDecl* D);
    SerializeResult build(FunctionDecl*    D);
    SerializeResult build(TypedefDecl*     D);
    SerializeResult build(TypeAliasDecl*   D);
    SerializeResult build(EnumDecl*        D);
    SerializeResult build(VarDecl*         D);

    template<class DeclTy>
    SerializeResult buildNamespace(DeclTy* D);

    template<class DeclTy>
    SerializeResult buildRecord(DeclTy* D);

    template<class DeclTy>
    SerializeResult buildFunction(DeclTy* D);

    template<class DeclTy>
    SerializeResult buildTypedef(DeclTy* D);

    template<class DeclTy>
    SerializeResult buildEnum(DeclTy* D);

    template<class DeclTy>
    SerializeResult buildVar(DeclTy* D);

    int
    getLine(
        NamedDecl const* D) const;

    template<typename DeclTy>
    void extract(DeclTy* D);

public:
    void HandleTranslationUnit(ASTContext& Context) override;

    bool shouldExtract(Decl const* D);
    bool WalkUpFromNamespaceDecl(NamespaceDecl* D);
    bool WalkUpFromCXXRecordDecl(CXXRecordDecl* D);
    bool WalkUpFromCXXMethodDecl(CXXMethodDecl* D);
    bool WalkUpFromFriendDecl(FriendDecl* D);
    //bool WalkUpFromUsingDecl(UsingDecl* D);
    //bool WalkUpFromUsingShadowDecl(UsingShadowDecl* D);
    bool WalkUpFromFunctionDecl(FunctionDecl* D);
    bool WalkUpFromTypeAliasDecl(TypeAliasDecl* D);
    bool WalkUpFromTypedefDecl(TypedefDecl* D);
    bool WalkUpFromEnumDecl(EnumDecl* D);
    bool WalkUpFromVarDecl(VarDecl* D);
};

} // mrdox
} // clang

#endif
