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
#include <mrdox/MetadataFwd.hpp>
#include <clang/Sema/SemaConsumer.h>
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
    : public SemaConsumer
{
public:
    struct FileFilter
    {
        std::string prefix;
        bool include = true;
    };

    ExecutionContext& ex_;
    ConfigImpl const& config_;
    clang::CompilerInstance& compiler_;
    Diagnostics diags_;

    ASTContext* astContext_ = nullptr;
    SourceManager* sourceManager_ = nullptr;
    Sema* sema_ = nullptr;

    llvm::SmallString<512> File_;
    bool IsFileInRootDir_;

    llvm::SmallString<128> usr_;

    std::unordered_map<
        clang::SourceLocation::UIntTy,
        FileFilter> fileFilter_;

    std::unordered_map<
        const Decl*,
        bool> namespaceFilter_;

public:
    ASTVisitor(
        tooling::ExecutionContext& ex,
        ConfigImpl const& config,
        clang::CompilerInstance& compiler) noexcept;

    bool
    extractSymbolID(
        const Decl* D,
        SymbolID& id);

    SymbolID
    extractSymbolID(
        const Decl* D);

    bool shouldSerializeInfo(
        const NamedDecl* D) noexcept;

    bool
    shouldExtract(
        const Decl* D);

    bool
    extractInfo(
        Info& I,
        const NamedDecl* D);

    std::string
    extractName(
        const NamedDecl* D);

    int
    getLine(
        const NamedDecl* D) const;

    std::string
    getSourceCode(
        SourceRange const& R);

    std::string
    getTypeAsString(
        QualType T);

    template<typename TypeInfoTy>
    std::unique_ptr<TypeInfoTy>
    makeTypeInfo(
        const IdentifierInfo* II,
        unsigned quals);

    template<typename TypeInfoTy>
    std::unique_ptr<TypeInfoTy>
    makeTypeInfo(
        const NamedDecl* N,
        unsigned quals);

    std::unique_ptr<TypeInfo>
    buildTypeInfoForType(
        const NestedNameSpecifier* N);

    std::unique_ptr<TypeInfo>
    buildTypeInfoForType(
        QualType T,
        unsigned quals = 0);

    template<typename Integer>
    Integer
    getValue(const llvm::APInt& V);

    void
    buildExprInfoForExpr(
        ExprInfo& I,
        const Expr* E);

    template<typename T>
    void
    buildExprInfoForExpr(
        ConstantExprInfo<T>& I,
        const Expr* E);

    template<typename T>
    void
    buildExprInfoForExpr(
        ConstantExprInfo<T>& I,
        const Expr* E,
        const llvm::APInt& V);

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

    template<typename Range>
    void
    buildTemplateArgs(
        std::vector<TArg>& result,
        Range&& range);

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
        std::unique_ptr<Javadoc>& javadoc,
        Decl const* D);

    void
    parseEnumerators(
        EnumInfo& I,
        const EnumDecl* D);

    bool
    getParentNamespaces(
        std::vector<SymbolID>& Namespaces,
        const Decl* D);

    void
    buildSpecialization(
        SpecializationInfo& I,
        const ClassTemplateSpecializationDecl* P,
        const Decl* C);

    void extractBases(
        RecordInfo& I,
        CXXRecordDecl* D);

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
        VariableInfo& I,
        VarDecl* D);

    template<class DeclTy>
    void buildTypedef(
        TypedefInfo& I,
        DeclTy* D);

    bool
    shouldExtractNamespace(const Decl * D);

    // --------------------------------------------------------

    bool traverse(NamespaceDecl*);
    bool traverse(TypedefDecl*, AccessSpecifier);
    bool traverse(TypeAliasDecl*, AccessSpecifier, std::unique_ptr<TemplateInfo>&&);
    bool traverse(CXXRecordDecl*, AccessSpecifier, std::unique_ptr<TemplateInfo>&&);
    bool traverse(VarDecl*, AccessSpecifier, std::unique_ptr<TemplateInfo>&&);
    bool traverse(FunctionDecl*, AccessSpecifier, std::unique_ptr<TemplateInfo>&&);
    bool traverse(CXXMethodDecl*, AccessSpecifier, std::unique_ptr<TemplateInfo>&&);
    bool traverse(CXXConstructorDecl*, AccessSpecifier, std::unique_ptr<TemplateInfo>&&);
    bool traverse(CXXConversionDecl*, AccessSpecifier, std::unique_ptr<TemplateInfo>&&);
    bool traverse(CXXDeductionGuideDecl*, AccessSpecifier, std::unique_ptr<TemplateInfo>&&);
    // destructors cannot be templates
    bool traverse(CXXDestructorDecl*, AccessSpecifier);
    bool traverse(EnumDecl*, AccessSpecifier);
    bool traverse(FieldDecl*, AccessSpecifier);

    // KRYSTIAN TODO: friends are a can of worms
    // we do not wish to open just yet
    bool traverse(FriendDecl*);

    bool traverse(ClassTemplateDecl*, AccessSpecifier);
    bool traverse(ClassTemplateSpecializationDecl*);
    bool traverse(VarTemplateDecl*, AccessSpecifier);
    bool traverse(VarTemplateSpecializationDecl*);
    bool traverse(FunctionTemplateDecl*, AccessSpecifier);
    bool traverse(ClassScopeFunctionSpecializationDecl*);
    bool traverse(TypeAliasTemplateDecl*, AccessSpecifier);

#if 0
    // includes both linkage-specification forms in [dcl.link]:
    //     extern string-literal { declaration-seq(opt) }
    //     extern string-literal name-declaration
    bool traverse(LinkageSpecDecl*);
    bool traverse(ExternCContextDecl*);
    bool traverse(ExportDecl*);
#endif

    // catch-all function so overload resolution does not
    // cause a hard error in the Traverse function for Decl
    template<typename... Args>
    auto traverse(Args&&...);

    template<typename... Args>
    bool traverseDecl(Decl* D, Args&&... args);
    bool traverseContext(DeclContext* D);

    void HandleTranslationUnit(ASTContext& Context) override;

    /** Skip function bodies

        This is called by Sema when parsing a function that has a body and:
        - is constexpr, or
        - uses a placeholder for a deduced return type

        We always return `true` because whenever this function *is* called,
        it will be for a function that cannot be used in a constant expression,
        nor one that introduces a new type via returning a local class.
    */
    bool shouldSkipFunctionBody(Decl* D) override { return true; }

    void Initialize(ASTContext& Context) override;
    void InitializeSema(Sema& S) override;
    void ForgetSema() override;

    void HandleCXXStaticMemberVarInstantiation(VarDecl* D) override;
    void HandleCXXImplicitFunctionInstantiation(FunctionDecl* D) override;
};

} // mrdox
} // clang

#endif
