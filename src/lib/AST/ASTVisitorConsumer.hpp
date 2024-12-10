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

#ifndef MRDOCS_LIB_AST_ASTVISITORCONSUMER_HPP
#define MRDOCS_LIB_AST_ASTVISITORCONSUMER_HPP

#include "lib/Lib/ConfigImpl.hpp"
#include "lib/Lib/ExecutionContext.hpp"
#include <mrdocs/Platform.hpp>
#include <clang/Tooling/Tooling.h>
#include <clang/Sema/SemaConsumer.h>

namespace clang {
namespace mrdocs {

/** A consumer for visiting AST nodes and performing semantic analysis.

    The @ref ASTAction class uses the ASTVisitor to perform semantic
    analysis on the AST and convert AST nodes into Info types for
    the MrDocs Corpus. This class is derived from `clang::SemaConsumer`
    and is used to visit AST nodes and perform various semantic
    analyses and transformations.

    This is done by overriding the virtual functions of the
    `clang::SemaConsumer` class. The main function this class
    overrides is `HandleTranslationUnit`, which is called
    when the translation unit is complete.
 */
class ASTVisitorConsumer
    : public SemaConsumer
{
    const ConfigImpl& config_;
    ExecutionContext& ex_;
    CompilerInstance& compiler_;
    Sema* sema_ = nullptr;

public:
    ASTVisitorConsumer(
        const ConfigImpl& config,
        ExecutionContext& ex,
        CompilerInstance& compiler) noexcept
        : config_(config)
        , ex_(ex)
        , compiler_(compiler)
    {
    }

    /** Initialize the semantic consumer

        Initialize the semantic consumer with the Sema instance
        being used to perform semantic analysis on the abstract syntax
        tree.
      */
    void
    InitializeSema(Sema& S) override
    {
        // Sema should not have been initialized yet
        MRDOCS_ASSERT(!sema_);
        sema_ = &S;
    }

    /** Inform the semantic consumer that Sema is no longer available.

        This is called when the Sema instance is no longer available.
      */
    void
    ForgetSema() override
    {
        sema_ = nullptr;
    }

    /** Handle a TranslationUnit

        This method is called when the ASTs for entire
        translation unit have been parsed.

        It's the main entry point for the ASTVisitorConsumer.
        It initializes the diagnostics reporter, loads and caches
        source files into memory, and then creates an ASTVisitor
        to traverse the translation unit.

        All other `Handle*` methods called by when a specific type
        of declaration or definition is found is left as an empty stub.
      */
    void
    HandleTranslationUnit(ASTContext& Context) override;

    /** Handle the specified top-level declaration.

        This is called by the parser to process every
        top-level Decl*.

        @returns `true` to always continue parsing
     */
    bool
    HandleTopLevelDecl(DeclGroupRef) override
    {
        return true;
    }

    /** Handle a static member variable instantiation.

        This is called by the parser to process a static
        member variable instantiation.

        The default implementation is an empty stub.
        This implementation sets the declaration as implicit
        because implicitly instantiated definitions of non-inline
        static data members of class templates are added to
        the end of the TU DeclContext. As a result, Decl::isImplicit
        returns false for these VarDecls, so we manually set it here.

        @param D The declaration of the static member variable
     */
    void
    HandleCXXStaticMemberVarInstantiation(VarDecl* D) override
    {
        D->setImplicit();
    }

    /** Handle an implicit function instantiation.

        This is called by the parser to process an implicit
        function instantiation.

        At this point, the function does not have a body. Its body is
        instantiated at the end of the translation unit and passed to
        HandleTopLevelDecl.

        The default implementation is an empty stub.
        This implementation sets the declaration as implicit
        because implicitly instantiated definitions of member
        functions of class templates are added to the end of the
        TU DeclContext. As a result, Decl::isImplicit returns
        false for these FunctionDecls, so we manually set it here.

        @param D The declaration of the function
     */
    void
    HandleCXXImplicitFunctionInstantiation(FunctionDecl* D) override
    {
        D->setImplicit();
    }

    /** Handle an inline function definition.

        This is called by the parser to process an inline
        function definition.

        The implementation is an empty stub.

        @param D The declaration of the function
     */
    void HandleInlineFunctionDefinition(FunctionDecl*) override { }

    /** Handle a tag declaration definition.

        This is called by the parser to process a tag declaration
        definition.

        The implementation is an empty stub.

        @param D The declaration of the tag
     */
    void HandleTagDeclDefinition(TagDecl*) override { }

    /** Handle a tag declaration required definition.

        This is called by the parser to process a tag declaration
        required definition.

        The implementation is an empty stub.

        @param D The declaration of the tag
     */
    void HandleTagDeclRequiredDefinition(const TagDecl*) override { }

    /** Handle an interesting declaration.

        This is called by the AST reader when deserializing things
        that might interest the consumer.

        The default implementation forwards to HandleTopLevelDecl.

        The implementation is an empty stub.

        @param D The declaration
     */
    void HandleInterestingDecl(DeclGroupRef) override { }

    /** Handle a tentative definition.

        This is called by the parser to process a tentative definition.

        The implementation is an empty stub.

        @param D The declaration
     */
    void CompleteTentativeDefinition(VarDecl*) override { }

    /** Handle a tentative definition.

        This is called by the parser to process a tentative definition.

        The implementation is an empty stub.

        @param D The declaration
     */
    void CompleteExternalDeclaration(DeclaratorDecl*) override { }

    /** Handle a vtable.

        This is called by the parser to process a vtable.

        The implementation is an empty stub.

        @param D The declaration
     */
    void AssignInheritanceModel(CXXRecordDecl*) override { }

    /** Handle an implicit import declaration.

        This is called by the parser to process an implicit import declaration.

        The implementation is an empty stub.

        @param D The declaration
     */
    void HandleVTable(CXXRecordDecl*) override { }

    /** Handle an implicit import declaration.

        This is called by the parser to process an implicit import declaration.

        The implementation is an empty stub.

        @param D The declaration
     */
    void HandleImplicitImportDecl(ImportDecl*) override { }

    /** Handle a top-level declaration in an Objective-C container.

        This is called by the parser to process a top-level declaration
        in an Objective-C container.

        The implementation is an empty stub.

        @param D The declaration
     */
    void HandleTopLevelDeclInObjCContainer(DeclGroupRef) override { }
};

} // mrdocs
} // clang

#endif
