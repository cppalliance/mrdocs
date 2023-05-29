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

    clang::CompilerInstance& compiler_;

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

    Sema* sema_;

    std::size_t context_depth_ = 0;
    bool encountered_explicit_ = false;

public:
    ASTVisitor(
        tooling::ExecutionContext& ex,
        ConfigImpl const& config,
        clang::CompilerInstance& compiler,
        Reporter& R) noexcept;

    bool
    extractSymbolID(
        const Decl* D,
        SymbolID& id);

    SymbolID
    extractSymbolID(
        const Decl* D);

    bool shouldSerializeInfo(
        bool PublicOnly,
        bool IsOrIsInAnonymousNamespace,
        const NamedDecl* D) noexcept;

    bool
    getParentNamespaces(
        llvm::SmallVector<Reference, 4>& Namespaces,
        const Decl* D);

    bool
    shouldExtract(
        const Decl* D);

    bool
    extractInfo(
        Info& I,
        const NamedDecl* D);

    int
    getLine(
        const NamedDecl* D) const;

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
        TemplateInfo& I,
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
        TemplateInfo& I,
        const ClassTemplateSpecializationDecl* spec);

    void
    parseTemplateArgs(
        TemplateInfo& I,
        const VarTemplateSpecializationDecl* spec);

    void
    parseTemplateArgs(
        TemplateInfo& I,
        const FunctionTemplateSpecializationInfo* spec);

    void
    parseTemplateArgs(
        TemplateInfo& I,
        const ClassScopeFunctionSpecializationDecl* spec);

    void
    parseRawComment(
        std::optional<Javadoc>& javadoc,
        Decl const* D,
        Reporter& R);

    void
    parseEnumerators(
        EnumInfo& I,
        const EnumDecl* D);

    void extractBases(RecordInfo& I, CXXRecordDecl* D);

    template<class DeclTy>
    bool constructFunction(
        FunctionInfo& I,
        DeclTy* D);

    template<class DeclTy>
    void buildFunction(
        FunctionInfo& I,
        DeclTy* D);

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

    void buildField(
        FieldInfo& I,
        FieldDecl* D);

    void buildVar(
        VarInfo& I,
        VarDecl* D);

    template<class DeclTy>
    void buildTypedef(
        TypedefInfo& I,
        DeclTy* D);

    // --------------------------------------------------------

    bool Traverse(NamespaceDecl* D);
    bool Traverse(CXXRecordDecl* D,
        std::unique_ptr<TemplateInfo>&& Template = nullptr);
    bool Traverse(TypedefDecl* D);
    bool Traverse(TypeAliasDecl* D,
        std::unique_ptr<TemplateInfo>&& Template = nullptr);
    bool Traverse(VarDecl* D,
        std::unique_ptr<TemplateInfo>&& Template = nullptr);
    bool Traverse(FunctionDecl* D,
        std::unique_ptr<TemplateInfo>&& Template = nullptr);
    bool Traverse(CXXMethodDecl* D,
        std::unique_ptr<TemplateInfo>&& Template = nullptr);
    bool Traverse(CXXConstructorDecl* D,
        std::unique_ptr<TemplateInfo>&& Template = nullptr);
    bool Traverse(CXXConversionDecl* D,
        std::unique_ptr<TemplateInfo>&& Template = nullptr);
    bool Traverse(CXXDeductionGuideDecl* D,
        std::unique_ptr<TemplateInfo>&& Template = nullptr);
    // destructors cannot be templates
    bool Traverse(CXXDestructorDecl* D);
    bool Traverse(FriendDecl* D);
    bool Traverse(EnumDecl* D);
    bool Traverse(FieldDecl* D);

    bool Traverse(ClassTemplateDecl* D);
    bool Traverse(ClassTemplateSpecializationDecl* D);
    bool Traverse(ClassTemplatePartialSpecializationDecl* D);
    bool Traverse(VarTemplateDecl* D);
    bool Traverse(VarTemplateSpecializationDecl* D);
    bool Traverse(VarTemplatePartialSpecializationDecl* D);
    bool Traverse(FunctionTemplateDecl* D);
    bool Traverse(ClassScopeFunctionSpecializationDecl* D);
    bool Traverse(TypeAliasTemplateDecl* D);
#if 0
    // includes both linkage-specification forms in [dcl.link]:
    //     extern string-literal { declaration-seq(opt) }
    //     extern string-literal name-declaration
    bool Traverse(LinkageSpecDecl* D);
    bool Traverse(ExternCContextDecl* D);
    bool Traverse(ExportDecl* D);
#endif

    // catch-all function so overload resolution does not
    // cause a hard error in the Traverse function for Decl
    template<typename... Args>
    auto Traverse(Args&&...);

    template<typename... Args>
    bool TraverseDecl(Decl* D, Args&&... args);
    bool TraverseContext(DeclContext* D);

    void HandleTranslationUnit(ASTContext& Context) override;
};

} // mrdox
} // clang

#endif
