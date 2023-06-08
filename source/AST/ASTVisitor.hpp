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

#ifndef MRDOX_LIB_AST_ASTVISITOR_HPP
#define MRDOX_LIB_AST_ASTVISITOR_HPP

#include "ConfigImpl.hpp"
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
        llvm::SmallString<0> prefix;
        bool include = true;
    };

    tooling::ExecutionContext& ex_;
    ConfigImpl const& config_;

    llvm::SmallString<512> File_;
    bool IsFileInRootDir_;

    llvm::SmallString<128> usr_;

    std::unordered_map<
        clang::SourceLocation::UIntTy,
        FileFilter> fileFilter_;

    clang::CompilerInstance& compiler_;

    ASTContext* astContext_ = nullptr;
    SourceManager* sourceManager_ = nullptr;
    Sema* sema_ = nullptr;

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

    template<typename DeclTy>
    bool
    isImplicitInstantiation(
        const DeclTy* D);

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
        AccessSpecifier A,
        DeclTy* D);

    template<class DeclTy>
    void buildFunction(
        FunctionInfo& I,
        AccessSpecifier A,
        DeclTy* D);

    void buildRecord(
        RecordInfo& I,
        AccessSpecifier A,
        CXXRecordDecl* D);

    void buildNamespace(
        NamespaceInfo& I,
        NamespaceDecl* D);

    void buildFriend(
        FriendDecl* D);

    void buildEnum(
        EnumInfo& I,
        AccessSpecifier A,
        EnumDecl* D);

    void buildField(
        FieldInfo& I,
        AccessSpecifier A,
        FieldDecl* D);

    void buildVar(
        VarInfo& I,
        AccessSpecifier A,
        VarDecl* D);

    template<class DeclTy>
    void buildTypedef(
        TypedefInfo& I,
        AccessSpecifier A,
        DeclTy* D);

    // --------------------------------------------------------

    bool Traverse(NamespaceDecl*);
    bool Traverse(TypedefDecl*, AccessSpecifier);
    bool Traverse(TypeAliasDecl*, AccessSpecifier, std::unique_ptr<TemplateInfo>&&);
    bool Traverse(CXXRecordDecl*, AccessSpecifier, std::unique_ptr<TemplateInfo>&&);
    bool Traverse(VarDecl*, AccessSpecifier, std::unique_ptr<TemplateInfo>&&);
    bool Traverse(FunctionDecl*, AccessSpecifier, std::unique_ptr<TemplateInfo>&&);
    bool Traverse(CXXMethodDecl*, AccessSpecifier, std::unique_ptr<TemplateInfo>&&);
    bool Traverse(CXXConstructorDecl*, AccessSpecifier, std::unique_ptr<TemplateInfo>&&);
    bool Traverse(CXXConversionDecl*, AccessSpecifier, std::unique_ptr<TemplateInfo>&&);
    bool Traverse(CXXDeductionGuideDecl*, AccessSpecifier, std::unique_ptr<TemplateInfo>&&);
    // destructors cannot be templates
    bool Traverse(CXXDestructorDecl*, AccessSpecifier);
    bool Traverse(EnumDecl*, AccessSpecifier);
    bool Traverse(FieldDecl*, AccessSpecifier);

    // KRYSTIAN TODO: friends are a can of worms
    // we do not wish to open just yet
    bool Traverse(FriendDecl*);

    bool Traverse(ClassTemplateDecl*, AccessSpecifier);
    bool Traverse(ClassTemplateSpecializationDecl*);
    bool Traverse(VarTemplateDecl*, AccessSpecifier);
    bool Traverse(VarTemplateSpecializationDecl*);
    bool Traverse(FunctionTemplateDecl*, AccessSpecifier);
    bool Traverse(ClassScopeFunctionSpecializationDecl*);
    bool Traverse(TypeAliasTemplateDecl*, AccessSpecifier);

#if 0
    // includes both linkage-specification forms in [dcl.link]:
    //     extern string-literal { declaration-seq(opt) }
    //     extern string-literal name-declaration
    bool Traverse(LinkageSpecDecl*);
    bool Traverse(ExternCContextDecl*);
    bool Traverse(ExportDecl*);
#endif

    // catch-all function so overload resolution does not
    // cause a hard error in the Traverse function for Decl
    template<typename... Args>
    auto Traverse(Args&&...);

    template<typename... Args>
    bool TraverseDecl(Decl* D, Args&&... args);
    bool TraverseContext(DeclContext* D);

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

};

} // mrdox
} // clang

#endif
