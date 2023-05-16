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

#include "api/ConfigImpl.hpp"
#include <mrdox/Reporter.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <mrdox/Metadata/Access.hpp>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Tooling/Execution.h>
#include <llvm/ADT/Optional.h>
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

    llvm::SmallString<128> usr_;

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

    SymbolID getSymbolID(Decl const* D);

    bool shouldSerializeInfo(
        bool PublicOnly,
        bool IsInAnonymousNamespace,
        NamedDecl const* D) noexcept;

    void
    getParent(
        SymbolID& parent,
        Decl const* D);

    void
    getParentNamespaces(
        llvm::SmallVector<Reference, 4>& Namespaces,
        Decl const* D,
        bool& IsInAnonymousNamespace);

    std::string
    getSourceCode(
        Decl const* D,
        SourceRange const& R);

    std::string
    getTypeAsString(
        QualType T);

    Access
    getAccessFromSpecifier(
        AccessSpecifier as) noexcept;

    TagDecl*
    getTagDeclForType(
        QualType T);

    RecordDecl*
    getRecordDeclForType(
        QualType T);

    TypeInfo
    getTypeInfoForType(
        QualType T);

    void
    parseParameters(
        FunctionInfo& I,
        FunctionDecl const* D);

    void
    getTemplateParams(
        llvm::Optional<TemplateInfo>& TemplateInfo,
        const Decl* D);
    void
    parseRawComment(
        llvm::Optional<Javadoc>& javadoc,
        Decl const* D,
        Reporter& R);

    void
    getMemberTypeInfo(
        MemberTypeInfo& I,
        FieldDecl const* D,
        Reporter& R);

    void
    parseFields(
        RecordInfo& I,
        const RecordDecl* D,
        bool PublicOnly,
        AccessSpecifier Access,
        Reporter& R);

    void
    parseEnumerators(
        EnumInfo& I,
        const EnumDecl* D);

public://private:
    bool shouldExtract(Decl const* D);
    bool extractSymbolID(SymbolID& id, NamedDecl const* D);
    bool extractInfo(Info& I, NamedDecl const* D);
    Reference getFunctionReference(FunctionDecl const* D);
    int getLine(NamedDecl const* D) const;
    void extractBases(RecordInfo& I, CXXRecordDecl* D);

private:
    template<class DeclTy>
    bool constructFunction(
        FunctionInfo& I, DeclTy* D, char const* name = nullptr);

    void buildNamespace (NamespaceDecl* D);
    void buildRecord    (CXXRecordDecl* D);
    void buildFriend    (FriendDecl* D);
    void buildEnum      (EnumDecl* D);
    void buildVar       (VarDecl* D);

    template<class DeclTy> void buildTypedef    (DeclTy* D);

    template<class DeclTy>
    requires std::derived_from<DeclTy, CXXMethodDecl>
    void buildFunction(DeclTy* D, char const* name = nullptr);

    template<class DeclTy>
    requires (! std::derived_from<DeclTy, CXXMethodDecl>)
    void buildFunction(DeclTy* D);

public:
    bool shouldTraversePostOrder() const noexcept { return true; }
    void HandleTranslationUnit(ASTContext& Context) override;
    bool WalkUpFromNamespaceDecl(NamespaceDecl* D);
    bool WalkUpFromCXXRecordDecl(CXXRecordDecl* D);
    bool WalkUpFromCXXMethodDecl(CXXMethodDecl* D);
    bool WalkUpFromCXXDestructorDecl(CXXDestructorDecl* D);
    bool TraverseCXXConstructorDecl(CXXConstructorDecl* D);
    bool WalkUpFromCXXConversionDecl(CXXConversionDecl* D);
    bool WalkUpFromFunctionDecl(FunctionDecl* D);
    bool WalkUpFromFriendDecl(FriendDecl* D);
    bool WalkUpFromTypeAliasDecl(TypeAliasDecl* D);
    bool WalkUpFromTypedefDecl(TypedefDecl* D);
    bool WalkUpFromEnumDecl(EnumDecl* D);
    bool WalkUpFromVarDecl(VarDecl* D);
    bool WalkUpFromParmVarDecl(ParmVarDecl* D);
};

} // mrdox
} // clang

#endif
