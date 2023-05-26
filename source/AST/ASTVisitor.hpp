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

#ifndef MRDOX_API_AST_ASTVISITOR_HPP
#define MRDOX_API_AST_ASTVISITOR_HPP

#include "ConfigImpl.hpp"
#include <mrdox/Reporter.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <mrdox/Metadata/Access.hpp>
#include <clang/Tooling/Execution.h>
#include <optional>
#include <unordered_map>

namespace clang {
namespace mrdox {

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
    : public ASTConsumer
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

    CXXRecordDecl*
    getCXXRecordDeclForType(
        QualType T);

    TypeInfo
    getTypeInfoForType(
        QualType T);

    void
    parseParameters(
        FunctionInfo& I,
        FunctionDecl const* D);

    void
    applyDecayToParameters(
        FunctionDecl* D);

    void
    parseTemplateParams(
        std::optional<TemplateInfo>& TemplateInfo,
        const Decl* D);

    TParam
    buildTemplateParam(
        const NamedDecl* ND);

    template<typename R>
    void
    buildTemplateArgs(
        TemplateInfo& I,
        R&& args);

    void
    parseTemplateArgs(
        std::optional<TemplateInfo>& I,
        const ClassTemplateSpecializationDecl* spec);

    void
    parseTemplateArgs(
        std::optional<TemplateInfo>& I,
        const FunctionTemplateSpecializationInfo* spec);

    void
    parseTemplateArgs(
        std::optional<TemplateInfo>& I,
        const ClassScopeFunctionSpecializationDecl* spec);

    void
    parseRawComment(
        std::optional<Javadoc>& javadoc,
        Decl const* D,
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
    int getLine(NamedDecl const* D) const;
    void extractBases(RecordInfo& I, CXXRecordDecl* D);

private:
    template<class DeclTy>
    bool constructFunction(
        FunctionInfo& I,
        DeclTy* D,
        char const* name = nullptr);

    template<class DeclTy>
    void buildFunction(
        FunctionInfo& I,
        DeclTy* D,
        const char* name = nullptr);

    template<class DeclTy>
    void buildFunction(
        DeclTy* D,
        const char* name = nullptr);

    void buildRecord(
        RecordInfo& I,
        CXXRecordDecl* D);

    void buildNamespace(
        NamespaceInfo& I,
        NamespaceDecl* D);

    void buildFriend(
        FriendDecl* D);

    void buildEnum(
        EnumInfo& I,
        EnumDecl* D);

    void buildVar(
        VarInfo& I,
        VarDecl* D);

    template<class DeclTy>
    void buildTypedef(
        TypedefInfo& I,
        DeclTy* D);

public:
    void HandleTranslationUnit(ASTContext& Context) override;
    bool TraverseDecl(Decl* D);
    bool TraverseDeclContext(DeclContext* D);
    bool TraverseTranslationUnitDecl(TranslationUnitDecl* D);
    bool TraverseNamespaceDecl(NamespaceDecl* D);
    bool TraverseCXXRecordDecl(CXXRecordDecl* D);
    bool TraverseCXXMethodDecl(CXXMethodDecl* D);
    bool TraverseCXXDestructorDecl(CXXDestructorDecl* D);
    bool TraverseCXXConstructorDecl(CXXConstructorDecl* D);
    bool TraverseCXXConversionDecl(CXXConversionDecl* D);
    bool TraverseCXXDeductionGuideDecl(CXXDeductionGuideDecl* D);
    bool TraverseFunctionDecl(FunctionDecl* D);
    bool TraverseFriendDecl(FriendDecl* D);
    bool TraverseTypeAliasDecl(TypeAliasDecl* D);
    bool TraverseTypedefDecl(TypedefDecl* D);
    bool TraverseEnumDecl(EnumDecl* D);
    bool TraverseVarDecl(VarDecl* D);
    bool TraverseClassTemplateDecl(ClassTemplateDecl* D);
    bool TraverseClassTemplateSpecializationDecl(ClassTemplateSpecializationDecl* D);
    bool TraverseClassTemplatePartialSpecializationDecl(ClassTemplatePartialSpecializationDecl* D);
    bool TraverseFunctionTemplateDecl(FunctionTemplateDecl* D);
    bool TraverseClassScopeFunctionSpecializationDecl(ClassScopeFunctionSpecializationDecl* D);

#if 0
    // includes both linkage-specification forms in [dcl.link]:
    //     extern string-literal { declaration-seq(opt) }
    //     extern string-literal name-declaration
    bool TraverseLinkageSpecDecl(LinkageSpecDecl* D);
    bool TraverseExternCContextDecl(ExternCContextDecl* D);
    bool TraverseExportDecl(ExportDecl* D);
#endif
};

} // mrdox
} // clang

#endif
