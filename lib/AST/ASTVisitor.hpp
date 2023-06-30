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
#include <mrdox/Metadata/Info.hpp>
#include <clang/Sema/SemaConsumer.h>
#include <clang/Tooling/Execution.h>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace clang {
namespace mrdox {

using InfoPtr = std::unique_ptr<Info>;

struct InfoPtrHasher
{
    using is_transparent = void;

    std::size_t operator()(
        const InfoPtr& I) const;

    std::size_t operator()(
        const SymbolID& id) const;
};

struct InfoPtrEqual
{
    using is_transparent = void;

    bool operator()(
        const InfoPtr& a,
        const InfoPtr& b) const;

    bool operator()(
        const InfoPtr& a,
        const SymbolID& b) const;

    bool operator()(
        const SymbolID& a,
        const InfoPtr& b) const;
};

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

    std::unordered_set<
        InfoPtr,
        InfoPtrHasher,
        InfoPtrEqual> info_;

    // KRYSTIAN FIXME: this is terrible
    bool forceExtract_ = false;

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

    std::string
    extractName(
        const NamedDecl* D);

    Info* getInfo(const SymbolID& id);

    template<typename InfoTy>
    InfoTy&
    createInfo(const SymbolID& id);

    template<typename InfoTy>
    std::pair<InfoTy&, bool>
    getOrCreateInfo(const SymbolID& id);

    Info&
    getOrBuildInfo(Decl* D);

    template<
        typename InfoTy,
        typename Child>
    void
    emplaceChild(
        InfoTy& I,
        Child&& C);

    unsigned
    getLine(
        const NamedDecl* D) const;

    void
    addSourceLocation(
        SourceInfo& I,
        unsigned line,
        bool definition);

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
        NamedDecl* N,
        unsigned quals);

    std::unique_ptr<TypeInfo>
    buildTypeInfo(
        const NestedNameSpecifier* N);

    std::unique_ptr<TypeInfo>
    buildTypeInfo(
        QualType T,
        unsigned quals = 0);

    template<typename Integer>
    Integer
    getValue(const llvm::APInt& V);

    void
    buildExprInfo(
        ExprInfo& I,
        const Expr* E);

    template<typename T>
    void
    buildExprInfo(
        ConstantExprInfo<T>& I,
        const Expr* E);

    template<typename T>
    void
    buildExprInfo(
        ConstantExprInfo<T>& I,
        const Expr* E,
        const llvm::APInt& V);

    void
    applyDecayToParameters(
        const FunctionDecl* D);

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

    void
    getParentNamespaces(
        Info& I,
        Decl* D);

    void buildSpecialization(
        SpecializationInfo& I,
        bool created,
        ClassTemplateSpecializationDecl* D);

    void buildNamespace(
        NamespaceInfo& I,
        bool created,
        NamespaceDecl* D);

    void buildRecord(
        RecordInfo& I,
        bool created,
        CXXRecordDecl* D);

    void buildEnum(
        EnumInfo& I,
        bool created,
        EnumDecl* D);

    void buildTypedef(
        TypedefInfo& I,
        bool created,
        TypedefNameDecl* D);

    void buildVariable(
        VariableInfo& I,
        bool created,
        VarDecl* D);

    void buildField(
        FieldInfo& I,
        bool created,
        FieldDecl* D);

    template<class DeclTy>
    void buildFunction(
        FunctionInfo& I,
        bool created,
        DeclTy* D);

    void buildFriend(
        FriendDecl* D);

    // --------------------------------------------------------

    bool traverse(NamespaceDecl*);
    bool traverse(CXXRecordDecl*, AccessSpecifier, std::unique_ptr<TemplateInfo>&&);
    bool traverse(EnumDecl*, AccessSpecifier);
    bool traverse(TypedefDecl*, AccessSpecifier);
    bool traverse(TypeAliasDecl*, AccessSpecifier, std::unique_ptr<TemplateInfo>&&);
    bool traverse(VarDecl*, AccessSpecifier, std::unique_ptr<TemplateInfo>&&);
    bool traverse(FieldDecl*, AccessSpecifier);
    bool traverse(FunctionDecl*, AccessSpecifier, std::unique_ptr<TemplateInfo>&&);
    bool traverse(CXXMethodDecl*, AccessSpecifier, std::unique_ptr<TemplateInfo>&&);
    bool traverse(CXXConstructorDecl*, AccessSpecifier, std::unique_ptr<TemplateInfo>&&);
    bool traverse(CXXConversionDecl*, AccessSpecifier, std::unique_ptr<TemplateInfo>&&);
    bool traverse(CXXDeductionGuideDecl*, AccessSpecifier, std::unique_ptr<TemplateInfo>&&);
    // destructors cannot be templates
    bool traverse(CXXDestructorDecl*, AccessSpecifier);

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

    void Initialize(ASTContext& Context) override;
    void InitializeSema(Sema& S) override;
    void ForgetSema() override;
    
    /** Skip function bodies

        This is called by Sema when parsing a function that has a body and:
        - is constexpr, or
        - uses a placeholder for a deduced return type

        We always return `true` because whenever this function *is* called,
        it will be for a function that cannot be used in a constant expression,
        nor one that introduces a new type via returning a local class.
    */
    bool shouldSkipFunctionBody(Decl* D) override { return true; }

    void HandleCXXStaticMemberVarInstantiation(VarDecl* D) override;
    void HandleCXXImplicitFunctionInstantiation(FunctionDecl* D) override;
    
    void HandleTranslationUnit(ASTContext& Context) override;
};

} // mrdox
} // clang

#endif
