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

#ifndef MRDOCS_LIB_AST_ASTVISITOR_HPP
#define MRDOCS_LIB_AST_ASTVISITOR_HPP

#include "lib/Lib/ConfigImpl.hpp"
#include "lib/Lib/ExecutionContext.hpp"
#include "lib/AST/SymbolFilter.hpp"
#include "lib/AST/ClangHelpers.hpp"
#include <mrdocs/Metadata/Source.hpp>
#include <mrdocs/Metadata/Name.hpp>
#include <mrdocs/Metadata/Scope.hpp>
#include <clang/Tooling/Tooling.h>
#include <clang/AST/ODRHash.h>
#include <llvm/ADT/SmallBitVector.h>

namespace clang::mrdocs {

class TypeInfoBuilder;
class NameInfoBuilder;
template <class Derived>
class TerminalTypeVisitor;

/** Convert AST to metadata representation.

    This class is responsible for converting AST nodes
    in a `clang::ASTContext` into metadata representation
    for the MrDocs Corpus.

    It's instantiated by the `ASTVisitorConsumer` class
    created by the `ASTAction` class. The `ASTVisitorConsumer`
    only handles translation units represented by a
    `clang::ASTContext` by creating an instance of this
    class and calling the `build` method, which recursively
    traverses the `clang::Decl` representing the
    translation unit.

    As it traverse nodes, the `ASTVisitor` class will
    create MrDocs `Info` objects for each declaration that
    passes the filter configurations.
*/
class ASTVisitor
{
    friend TypeInfoBuilder;
    friend NameInfoBuilder;
    template <typename Derived>
    friend class TerminalTypeVisitor;

    // MrDocs configuration
    ConfigImpl const& config_;

    // MrDocs diagnostics
    Diagnostics diags_;

    // The compiler instance
    CompilerInstance& compiler_;

    // The AST context
    ASTContext& context_;

    // The source files in memory
    SourceManager& source_;

    // Semantic analysis
    Sema& sema_;

    // An unordered set of all extracted Info declarations
    InfoSet info_;

    /*  The set of dependencies found during extraction

        The metadata for these dependencies will be extracted
        after the initial extraction pass, if the configuration
        option `referencedDeclarations` is not set to `never`.

        @see @ref buildDependencies
     */
    std::unordered_set<Decl*> dependencies_;

    /* Struct to hold pre-processed file information.

        This struct stores information about a file, including its full path,
        short path, and file kind.

        The short path is the path of the file relative to a search directory
        used by the compiler.
     */
    struct FileInfo
    {
        static
        FileInfo
        build(
            std::span<const std::pair<std::string, FileKind>> search_dirs,
            std::string_view file_path,
            std::string_view sourceRoot);

        // The full path of the file.
        std::string full_path;

        // The file path relative to a search directory.
        std::string short_path;

        // The kind of the file.
        FileKind kind;
    };

    /*  A map of Clang FileEntry objects to Visitor FileInfo objects

        This map is used to store information about files
        that have been identified through the translation
        unit AST, including the full path to the file, the
        short path to the file relative to a search path,
        and the kind of file (source, system, etc.).

        This map is later used by the @ref buildFileInfo
        function to retrieve a pointer to a FileInfo
        object. This can be used later on to determine
        if a file should be extracted or to add the
        SourceInfo to an Info object.
    */
    std::unordered_map<const FileEntry*, FileInfo> files_;

    /* The filter for symbols to extract

        Whenever traversing a declaration, the ASTVisitor
        will check if the declaration passes the filter
        before extracting metadata for it.
     */
    SymbolFilter symbolFilter_;

    enum class ExtractMode
    {
        // Extraction of declarations which pass all filters
        Normal,
        // Extraction of declarations as direct dependencies
        DirectDependency,
        // Extraction of declarations as indirect dependencies
        IndirectDependency,
    };

    // The current extraction mode
    ExtractMode mode = ExtractMode::Normal;

    struct [[nodiscard]] ExtractionScope
    {
        ASTVisitor& visitor_;
        ExtractMode previous_;

    public:
        ExtractionScope(
            ASTVisitor& visitor,
            ExtractMode mode) noexcept
            : visitor_(visitor)
            , previous_(visitor.mode)
        {
            visitor_.mode = mode;
        }

        ~ExtractionScope()
        {
            visitor_.mode = previous_;
        }
    };

    ExtractionScope
    enterMode(ExtractMode new_mode) noexcept
    {
        return {*this, new_mode};
    }

    ExtractMode
    currentMode() const noexcept
    {
        return mode;
    }

    struct SFINAEInfo
    {
        TemplateDecl* Template = nullptr;
        const IdentifierInfo* Member = nullptr;
        ArrayRef<TemplateArgument> Arguments;
    };

    template <class InfoTy>
    struct upsertResult {
        InfoTy& I;
        bool isNew;
    };

public:
    /** Constructor for ASTVisitor.

        This constructor initializes the ASTVisitor with the given configuration,
        diagnostics, compiler instance, AST context, and Sema object.

        It also initializes clang custom documentation commands and
        populates `files_` with the FileInfo for files in the
        translation unit.

        @param config The configuration object.
        @param diags The diagnostics object.
        @param compiler The compiler instance.
        @param context The AST context.
        @param sema The Sema object.
     */
    ASTVisitor(
        const ConfigImpl& config,
        Diagnostics& diags,
        CompilerInstance& compiler,
        ASTContext& context,
        Sema& sema) noexcept;

    /** Build the metadata representation from the AST.

        This function initiates the process of converting AST nodes
        in the `clang::ASTContext` into metadata representation for
        the MrDocs Corpus. It recursively traverses the AST and
        extracts relevant information based on the filter configurations.

        It's the main entry point for the ASTVisitor class, and is
        called by the ASTVisitorConsumer class when the ASTs for
        the entire translation unit have been parsed.

        The translation unit declaration is processed with
        `traverseAny`, which will traverse all scopes and declarations
        recursively.

        If the configuration option `referencedDeclarations`
        is not set to `never`, a second pass is made to extract
        referenced declarations. The `buildDependencies` function
        is responsible for this second pass.
     */
    void
    build();

    /** Get the set of extracted Info declarations.

        This function returns a reference to the set of Info
        declarations that have been extracted by the ASTVisitor.

        @return A reference to the InfoSet containing the extracted Info declarations.
     */
    InfoSet&
    results()
    {
        return info_;
    }

private:
    /*  Build metadata representation for all dependencies in the AST

        If the option `referencedDeclarations` is not set to `never`,
        this function will be called after the initial extraction
        pass to extract metadata for all referenced declarations
        in the main symbols extracted.

        It calls `traverseAny` for each declaration in the set
        of dependencies, which will recursively traverse the
        declaration and extract the relevant information.

        If this pass finds new dependencies, it will call itself
        recursively to extract the metadata for those dependencies
        until no new dependencies are found.
     */
    void
    buildDependencies();

    // =================================================
    // AST Traversal
    // =================================================

    /*  Traverse a declaration

        This function is called to traverse any declaration.
        The Decl element is converted to its derived type,
        and the appropriate `traverse` overload is called.

        The `build()` function will call this function with
        `context_.getTranslationUnitDecl()` to initiate
        the traversal of the entire AST, while
        `buildDependencies()` will call it for each dependency.

        @param D The declaration to traverse.
     */
    void
    traverseAny(Decl* D);

    /*  Traverse a declaration and template declaration

        This function is called to traverse any declaration
        with the corresponding template declaration.

        The Decl element is converted to its derived type,
        and the appropriate `traverse` overload is called
        with the template declaration as the second argument.

        Both the information about the type and the
        template parameters are extracted into the
        `Info` object.

        The `traverseAny()` function will call this function
        for each declaration with a corresponding template
        declaration.

        @param D The declaration context to traverse.
        @param TD The corresponding template declaration.
     */
    template <std::derived_from<TemplateDecl> TemplateDeclTy>
    void
    traverseAny(Decl* D, TemplateDeclTy* TD);

    /*  The default implementation for the traverse function

        This function defines the usual behavior for the
        traverse function for a concrete declaration type.

        It creates a new corresponding Info object for the
        declaration, populates it with the necessary information,
        and optionally traverses the members of the declaration.

        @param D The declaration to traverse
     */
    template <class DeclTy>
    requires std::derived_from<DeclTy, Decl>
    void
    traverse(DeclTy* D)
    {
        defaultTraverseImpl<false>(D);
    }

    /*  The default implementation for the traverse function with a
        template parameter

        This function defines the usual behavior for the
        traverse function for a concrete declaration type
        when associated with a template declaration.

        It creates a new corresponding Info object for the
        declaration, populates it with the necessary information,
        populates the Template information, and optionally
        traverses the members of the declaration.

        @param D The declaration to traverse
        @param TD The template declaration associated with `D`
     */
    template <
        std::derived_from<Decl> DeclTy,
        std::derived_from<TemplateDecl> TemplateDeclTy>
    void
    traverse(DeclTy* D, TemplateDeclTy* TD)
    {
        defaultTraverseImpl<true>(D, TD);
    }

    /* Default implementation for the traverse function

        This function defines the common logic for
        `traverse(DeclTy*)` and `traverse(DeclTy*, TemplateDeclTy*)`,
        where the `PopulateFromTD` parameter determines whether
        the template information should be populated from the
        template declaration.
     */
    template <
        bool PopulateFromTD,
        std::derived_from<Decl> DeclTy,
        std::derived_from<TemplateDecl> TemplateDeclTy = TemplateDecl>
    void
    defaultTraverseImpl(DeclTy* D, TemplateDeclTy* TD = nullptr);

    /*  Traverse a C++ function or function specialization

        This handles `FunctionDecl` and `CXXMethodDecl`.

        Both these classes can return `true` for
        `D->isFunctionTemplateSpecialization()`,
        in which case the function will also populate the
        template parameters of the function.

     */
    template<class FunctionDeclTy>
    requires
        std::derived_from<FunctionDeclTy, Decl> &&
        std::derived_from<FunctionDeclTy, FunctionDecl>
    void
    traverse(FunctionDeclTy* D);

    /*  Traverse a using directive

        This function is called to traverse a using directive
        such as `using namespace std;`.

        If the parent declaration is a Namespace, we
        update its `UsingDirectives` field.
    */
    void
    traverse(UsingDirectiveDecl* D);

    /*  Traverse a member of an anonymous union.

        We get the anonymous union field and traverse it
        as a regular `FieldDecl`.
     */
    void
    traverse(IndirectFieldDecl*);

    // =================================================
    // AST Traversal Helpers
    // =================================================

    /*  Traverse the members of a declaration

        This function is called to traverse the members of
        a Decl that is a DeclContext with other members.

        The function will call traverseAny for all members of the
        declaration context.
    */
    template <std::derived_from<Decl> DeclTy>
    void
    traverseMembers(DeclTy* DC);

    /*  Generates a Unified Symbol Resolution value for a declaration.

        USRs are strings that provide an unambiguous reference to a symbol.

        This function determines the underlying Decl type and
        generates a USR for it.

        @returns true if USR generation succeeded.
    */
    Expected<SmallString<128>>
    generateUSR(const Decl* D) const;

    /*  Generate the symbol ID for a declaration.

        This function will extract the symbol ID for a
        declaration, and store it in the `id` input
        parameter.

        As USRs (Unified Symbol Resolution) could be
        large, especially for functions with long type
        arguments, we use 160-bits SHA1(USR) values.

        To guarantee the uniqueness of symbols while using
        a relatively small amount of memory (vs storing
        USRs directly), this function hashes the Decl
        USR value with SHA1.

        @return true if the symbol ID could be extracted.
     */
    bool
    generateID(const Decl* D, SymbolID& id) const;

    /*  Extracts the symbol ID for a declaration.

        This function will extract the symbol ID for a
        declaration, and return it.

        If the `generateID` function fails, the function
        will return SymbolID::invalid.

        @return the symbol ID for the declaration.
     */
    SymbolID
    generateID(const Decl* D) const;

    // =================================================
    // Populate functions
    // =================================================
    void
    populate(NamespaceInfo& I, bool isNew, NamespaceDecl* D);

    void
    populate(RecordInfo& I, bool isNew, CXXRecordDecl* D);

    template <std::derived_from<FunctionDecl> DeclTy>
    void
    populate(FunctionInfo& I, bool isNew, DeclTy* D);

    void
    populate(EnumInfo& I, bool isNew, EnumDecl* D);

    void
    populate(EnumConstantInfo& I, bool isNew, EnumConstantDecl* D);

    template<std::derived_from<TypedefNameDecl> TypedefNameDeclTy>
    void
    populate(TypedefInfo& I, bool isNew, TypedefNameDeclTy* D);

    void
    populate(VariableInfo& I, bool isNew, VarDecl* D);

    void
    populate(FieldInfo& I, bool isNew, FieldDecl* D);

    void
    populate(SpecializationInfo& I, bool isNew, ClassTemplateSpecializationDecl* D);

    void
    populate(FriendInfo& I, bool isNew, FriendDecl* D);

    void
    populate(GuideInfo& I, bool isNew, CXXDeductionGuideDecl* D);

    void
    populate(NamespaceAliasInfo& I, bool isNew, NamespaceAliasDecl* D);

    void
    populate(UsingInfo& I, bool isNew, UsingDecl* D);

    void
    populate(ConceptInfo& I, bool isNew, ConceptDecl* D);

    void
    populate(SourceInfo& I, clang::SourceLocation loc, bool definition, bool documented);

    /*  Default function to populate the template information

        This overload ignores the declaration and populates
        the template information with the template parameters
        of the template declaration.
     */
    template <
        std::derived_from<Decl> DeclTy,
        std::derived_from<TemplateDecl> TemplateDeclTy>
    void
    populate(TemplateInfo& Template, DeclTy* D, TemplateDeclTy* TD);

    /*  Populate the template information for a class template

        The function will populate the template parameters
        depending on whether the record is a specialization.
     */
    template<std::derived_from<CXXRecordDecl> CXXRecordDeclTy>
    void
    populate(TemplateInfo& Template, CXXRecordDeclTy*, ClassTemplateDecl* CTD);

    /*  Populate the template information for a variable template

        The function will populate the template parameters
        depending on whether the variable is a specialization.
    */
    template<std::derived_from<VarDecl> VarDeclTy>
    void
    populate(TemplateInfo& Template, VarDeclTy* D, VarTemplateDecl* VTD);

    void
    populate(NoexceptInfo& I, const FunctionProtoType* FPT);

    void
    populate(ExplicitInfo& I, const ExplicitSpecifier& ES);

    void
    populate(ExprInfo& I, const Expr* E);

    template <class T>
    void
    populate(ConstantExprInfo<T>& I, const Expr* E);

    template <class T>
    void
    populate(ConstantExprInfo<T>& I, const Expr* E, const llvm::APInt& V);

    void
    populate(std::unique_ptr<TParam>& I, const NamedDecl* N);

    void
    populate(TemplateInfo& TI, const TemplateParameterList* TPL);

    template <std::ranges::range Range>
    void
    populate(
        std::vector<std::unique_ptr<TArg>>& result,
        Range&& args)
    {
        for (TemplateArgument const& arg : args)
        {
            // KRYSTIAN NOTE: is this correct? should we have a
            // separate TArgKind for packs instead of "unlaminating"
            // them as we are doing here?
            if (arg.getKind() == TemplateArgument::Pack)
            {
                populate(result, arg.pack_elements());
            } else
            {
                result.emplace_back(toTArg(arg));
            }
        }
    }

    void
    populate(
        std::vector<std::unique_ptr<TArg>>& result,
        const ASTTemplateArgumentListInfo* args);

    // =================================================
    // Populate function helpers
    // =================================================
    // Extract the name of a declaration
    std::string
    extractName(NamedDecl const* D);

    // Extract the name of a declaration
    std::string
    extractName(DeclarationName N);

    /*  Populate the Info with its parent namespaces

        Given a Decl `D`, this function will populate
        the `Info` `I` with the SymbolID of each parent namespace
        of `D`. The SymbolID of the global namespace is always
        included as the first element of `I.Namespace`.
     */
    void
    populateNamespaces(Info& I, Decl* D);

    /*  Emplace a member Info into a ScopeInfo

        Given a ScopeInfo `P` and an Info `C`, this function will
        add the SymbolID of `C` to `P.Members` and `P.Lookups[C.Name]`.

        @param P The parent ScopeInfo
        @param C The child Info
     */
    static
    void
    addMember(
        ScopeInfo& P,
        Info& C);

    /*  Parse the comments above a declaration as Javadoc

        This function will parse the comments above a declaration
        as Javadoc, and store the results in the `javadoc` input
        parameter.

        @return true if the comments were successfully parsed as
        Javadoc, and false otherwise.
     */
    bool
    generateJavadoc(
        std::unique_ptr<Javadoc>& javadoc,
        Decl const* D);

    std::unique_ptr<TypeInfo>
    toTypeInfo(
        QualType qt,
        ExtractMode extract_mode = ExtractMode::IndirectDependency);

    std::unique_ptr<NameInfo>
    toNameInfo(
        NestedNameSpecifier const* NNS,
        ExtractMode extract_mode = ExtractMode::IndirectDependency);

    template <class TArgRange = ArrayRef<TemplateArgument>>
    std::unique_ptr<NameInfo>
    toNameInfo(
        DeclarationName Name,
        std::optional<TArgRange> TArgs = std::nullopt,
        const NestedNameSpecifier* NNS = nullptr,
        ExtractMode extract_mode = ExtractMode::IndirectDependency);

    template <class TArgRange = ArrayRef<TemplateArgument>>
    std::unique_ptr<NameInfo>
    toNameInfo(
        const Decl* D,
        std::optional<TArgRange> TArgs = std::nullopt,
        const NestedNameSpecifier* NNS = nullptr,
        ExtractMode extract_mode = ExtractMode::IndirectDependency);

    std::unique_ptr<TArg>
    toTArg(const TemplateArgument& A);

    // Pretty-print an expression
    std::string
    toString(const Expr* E);

    // Pretty-print a type
    std::string
    toString(const Type* T);

    template <class Integer>
    Integer
    toInteger(const llvm::APInt& V);

    /*  Get the original source code in a range

        This function is used to populate the information in
        Info types that contain expressions as written
        in the source code, such as the value of default
        arguments.
     */
    std::string
    getSourceCode(SourceRange const& R) const;

    // Determine if a type is a SFINAE type
    std::optional<std::pair<QualType, std::vector<TemplateArgument>>>
    isSFINAEType(QualType T);

    // Determine if a type is a SFINAE type
    std::optional<std::pair<QualType, std::vector<TemplateArgument>>>
    isSFINAEType(Type const* T)
    {
        return isSFINAEType(QualType(T, 0));
    }

    std::optional<
        std::tuple<TemplateParameterList*, llvm::SmallBitVector, unsigned>>
    isSFINAETemplate(TemplateDecl* TD, IdentifierInfo const* Member);

    static std::optional<SFINAEInfo>
    getSFINAETemplate(QualType T, bool AllowDependentNames);

    static
    std::optional<TemplateArgument>
    tryGetTemplateArgument(
        TemplateParameterList* Parameters,
        ArrayRef<TemplateArgument> Arguments,
        unsigned Index);

    // =================================================
    // Filters
    // =================================================

    /*  Determine if a declaration should be extracted

        This function will determine whether a declaration
        should be extracted based on the current extraction
        mode, and the current symbol filter state.

        The function filters private symbols, symbols outside
        the input files, and symbols in files that do not match
        the input file patterns.

        Any configuration options that affect extraction
        will be checked here.

        @param D the declaration to check
        @param access the access specifier of the declaration

        @return true if the declaration should be extracted,
        and false otherwise.
     */
    bool
    shouldExtract(const Decl* D, AccessSpecifier access);

    // Determine if a declaration passes the symbol filter
    bool
    checkSymbolFilter(const NamedDecl* ND);

    // Determine if a declaration is in a file that should be extracted
    bool
    inExtractedFile(const Decl* D);

    /*  Determine if the declaration is in a special namespace

        This function will determine if a declaration is in a
        special namespace based on the current symbol filter
        state and the special namespace patterns.

        The function will check if the declaration is in a
        special namespace, and if so, it will return true.

        @param D the declaration to check
        @param Patterns the special namespace patterns

        @return true if the declaration is in a special namespace,
        and false otherwise.
     */
    static
    bool
    isInSpecialNamespace(
        const Decl* D,
        std::span<const FilterPattern> Patterns);

    // @overload isInSpecialNamespace
    static
    bool
    isInSpecialNamespace(
        const NestedNameSpecifier* NNS,
        std::span<const FilterPattern> Patterns);

    /* Check if a declaration is in a special namespace

        This function will check if a declaration is in a special
        namespace based on the current symbol filter state and
        the special namespace patterns.

        If the declaration is in a special namespace, the function
        will set the NameInfo `I` to the appropriate special
        namespace name (see-below or implementation-defined), and
        return true.

        @param I The NameInfo to set if the declaration is in a
        special namespace
        @param NNS the nested name specifier of the declaration
        @param D the declaration to check

        @return true if the declaration is in a special namespace,
        and false otherwise.
     */
    bool
    checkSpecialNamespace(
        std::unique_ptr<NameInfo>& I,
        const NestedNameSpecifier* NNS,
        const Decl* D) const;

    // @overload checkSpecialNamespace
    bool
    checkSpecialNamespace(
        std::unique_ptr<TypeInfo>& I,
        const NestedNameSpecifier* NNS,
        const Decl* D) const;

    // =================================================
    // Element access
    // =================================================

    /*  Get Info from ASTVisitor InfoSet
     */
    Info*
    find(SymbolID const& id);

    /* Get FileInfo from ASTVisitor files

        This function will return a pointer to a FileInfo
        object for a given Clang FileEntry object.

        If the FileInfo object does not exist, the function
        will construct a new FileInfo object and add it to
        the files_ map.

        The map of files is created during the object
        construction, and is populated with the FileInfo
        for each file in the translation unit.

        @param file The Clang FileEntry object to get the FileInfo for.

        @return a pointer to the FileInfo object.
     */
    FileInfo*
    findFileInfo(clang::SourceLocation loc);

    /*  Get or construct an empty Info with a specified id.

        If an Info object for the declaration has already
        been extracted, this function will return a reference
        to the Info object.

        Otherwise, it will construct a new Info object of type
        `InfoTy` with the given `id`, and return a reference
        to the new Info object.
    */
    template <std::derived_from<Info> InfoTy>
    upsertResult<InfoTy>
    upsert(SymbolID const& id);

    /*  Get or construct an empty Info for a declaration.

        The function will determine if the declaration should
        be extracted, determine the symbol ID for the declaration,
        and, if there are no errors, call `upsert` with the
        symbol ID.

        The return type for the result is inferred from the
        declaration type.

        @param D The declaration to extract
     */
    template <std::derived_from<Decl> DeclType>
    Expected<upsertResult<MrDocsType_t<DeclType>>>
    upsert(DeclType* D);

    /* Get or construct an empty Info for a dependency declaration.

        The function will determine the symbol ID for the
        declaration, store it in the `id` input parameter,
        and add it to the set of dependencies.

        The dependencies are processed after the initial
        extraction pass, if the configuration option
        `referencedDeclarations` is not set to `never`.
     */
    void
    upsertDependency(Decl* D, SymbolID& id);

    // @overload upsertDependency
    void
    upsertDependency(Decl const* D, SymbolID& id)
    {
        return upsertDependency(const_cast<Decl*>(D), id);
    }
};

} // clang::mrdocs

#endif
