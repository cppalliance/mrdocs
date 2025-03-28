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
#include "lib/AST/ClangHelpers.hpp"
#include <mrdocs/Metadata/ExtractionMode.hpp>
#include <mrdocs/Metadata/Source.hpp>
#include <mrdocs/Metadata/Name.hpp>
#include <mrdocs/Support/Concepts.hpp>
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

    /*  The symbols we would extract if they were documented

        When `extract-all` is false, we only extract symbols
        that are documented. If a symbol reappears in the
        translation unit, we only extract the declaration
        that's documented.

        When `extract-all` is false and `warn-if-undocumented`
        is true, we also warn if a symbol is not documented.
        However, because a symbol can appear multiple times
        in multiple translation units, we cannot be sure
        a symbol is undocumented until we have processed
        all translation units.

        For this reason, this set stores the symbols that
        are not documented but would otherwise have been
        extracted as regular symbols in the current
        translation unit. After symbols from all translation
        units are merged, we will iterate these symbols
        and warn if they are not documented.
     */
    UndocumentedInfoSet undocumented_;

    /* Struct to hold pre-processed file information.

        This struct stores information about a file, including its full path,
        short path, and file kind.

        The short path is the path of the file relative to a search directory
        used by the compiler.
     */
    struct FileInfo
    {
        // The full path of the file.
        std::string full_path;

        // The file path relative to a search directory.
        std::string short_path;

        // The file path relative to the source-root directory.
        std::string source_path;

        // Whether this file passes the file filters
        std::optional<bool> passesFilters;
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
    llvm::DenseMap<FileID, FileInfo> files_;

    /*  Determine how a Decl matched the filters
     */
    enum class ExtractionMatchType {
        // It matches one of the patterns as is
        Strict,
        // It matches the prefix of a pattern, meaning
        // children of this symbol might be included
        Prefix,
        // A parent of this symbol matches a literal pattern,
        // meaning `parent::**`.
        LiteralParent,
        // The symbol is included because the parent is
        // included and the parent is not a prefix.
        StrictParent
    };

    /*  Extraction Info

        This struct is used to store information about
        the filters a Decl pass.
     */
    struct ExtractionInfo
    {
        // The extraction mode for this symbol
        ExtractionMode mode;

        // Whether this is extraction mode is due to
        // it matching a prefix of another symbol.
        ExtractionMatchType kind;
    };

    /*  A map of Clang Decl objects to ExtractionMode values

        This map is used to store the extraction mode for
        declarations that have been identified through the
        translation unit AST.

        This map is later used by the @ref shouldExtract
        function to determine if a declaration should be
        extracted based on the extraction mode.
    */
    std::unordered_map<Decl const*, ExtractionInfo> extraction_;

    /* How we should traverse the current node
     */
    enum TraversalMode {
        /*  Only symbols that pass the filters will be extracted

            Excluded symbols are not traversed. All other symbols
            are traversed and their appropriate ExtractionMode
            is determined.

            Member of these symbols are also traversed.

         */
        Regular,

        /*  All symbols are extracted

            Even excluded symbols are traversed. If a symbol
            doesn't pass the filters, it is extracted as a
            dependency.

            Members of these dependency symbols are not
            traversed.

            This is used when an included symbol makes a
            reference to an excluded symbol.
         */
        Dependency,

        /*  All symbols will be extracted and traversed

            This mode is used to extract all symbols, including
            those that are excluded by the filters. These
            excluded symbols are marked as dependencies.

            However, members of these dependency symbols are
            also traversed.

            This is used when an included record makes use
            of an excluded record as a base class.
            In this case, we also need information about the
            excluded base class members to populate the
            derived class.
         */
        BaseClass
    };

    /* The current traversal mode

        This defines the traversal mode assigned to
        new Info types.

        A symbol that passes the filters will be
        extracted as a regular symbol.

        If a symbol also passes the see-below or
        implementation-defined filters, we
        change the current traversal mode
        accordingly so the symbol can be tagged.
        Members of this symbol types are not
        extracted.

        A symbol that doesn't pass the filters
        can only be extracted as a dependency.
        A symbol is extracted as a dependency
        when another symbol makes a reference
        to it.
     */
    TraversalMode mode_ = Regular;

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
        ConfigImpl const& config,
        Diagnostics const& diags,
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

    /** Get the set of extracted Info declarations.

        This function returns a reference to the set of Info
        declarations that have been extracted by the ASTVisitor.

        @return A reference to the InfoSet containing the extracted Info declarations.
     */
    UndocumentedInfoSet&
    undocumented()
    {
        return undocumented_;
    }

private:
    // =================================================
    // AST Traversal
    // =================================================

    /*  The default implementation for the traverse function

        This function defines the usual behavior for the
        traverse function for a concrete declaration type.

        It creates a new corresponding Info object for the
        declaration, populates it with the necessary information,
        and optionally traverses the members of the declaration.

        @tparam InfoTy The type of Info object to create. If the
        default type is used, the function will determine the
        appropriate Info type based on the declaration type.
        @tparam DeclTy The type of the declaration to traverse.

        @param D The declaration to traverse
     */
    template <
        class InfoTy = void,
        std::derived_from<Decl> DeclTy>
    Info*
    traverse(DeclTy const* D);

    /*  Traverse a function template

        This function redirects the traversal
        so that it can be handled as a
        concept or function template.

     */
    Info*
    traverse(FunctionTemplateDecl const* D);

    /*  Traverse a using directive

        This function is called to traverse a using directive
        such as `using namespace std;`.

        If the parent declaration is a Namespace, we
        update its `UsingDirectives` const field.
    */
    Info*
    traverse(UsingDirectiveDecl const* D);

    /*  Traverse a member of an anonymous union.

        We get the anonymous union field and traverse it
        as a regular `FieldDecl`.
     */
    Info*
    traverse(IndirectFieldDecl const* D);

    // =================================================
    // AST Traversal Helpers
    // =================================================

    /*  Traverse the members of a declaration

        This function is called to traverse the members of
        a Decl that is a DeclContext with other members.

        The function will call traverseAny for all members of the
        declaration context.
    */
    template <
        std::derived_from<Info> InfoTy,
        std::derived_from<Decl> DeclTy>
    requires (!std::derived_from<DeclTy, RedeclarableTemplateDecl>)
    void
    traverseMembers(InfoTy& I, DeclTy const* DC);

    template <
        std::derived_from<Info> InfoTy,
        std::derived_from<RedeclarableTemplateDecl> DeclTy>
    void
    traverseMembers(InfoTy& I, DeclTy const* DC);

    /*  Traverse the parents of a declaration

        This function is called to traverse the parents of
        a Decl until we find the translation unit declaration
        or a parent that has already been extracted.

        This function is called when the declaration is
        being extracted as a dependency, and we need to
        identify the parent scope of the declaration.

        For regular symbols, the parent is identified
        during the normal traversal of the declaration
        context.
    */
    template <
        std::derived_from<Info> InfoTy,
        std::derived_from<Decl> DeclTy>
    requires (!std::derived_from<DeclTy, RedeclarableTemplateDecl>)
    void
    traverseParent(InfoTy& I, DeclTy const* DC);

    template <
        std::derived_from<Info> InfoTy,
        std::derived_from<RedeclarableTemplateDecl> DeclTy>
    void
    traverseParent(InfoTy& I, DeclTy const* DC);

    /*  Generates a Unified Symbol Resolution value for a declaration.

        USRs are strings that provide an unambiguous reference to a symbol.

        This function determines the underlying Decl type and
        generates a USR for it.

        @returns true if USR generation succeeded.
    */
    Expected<SmallString<128>>
    generateUSR(Decl const* D) const;

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
    generateID(Decl const* D, SymbolID& id) const;

    /*  Extracts the symbol ID for a declaration.

        This function will extract the symbol ID for a
        declaration, and return it.

        If the `generateID` function fails, the function
        will return SymbolID::invalid.

        @return the symbol ID for the declaration.
     */
    SymbolID
    generateID(Decl const* D) const;

    // =================================================
    // Populate functions
    // =================================================
    template <std::derived_from<Decl> DeclTy>
    void
    populate(Info& I, bool isNew, DeclTy const* D);

    template <std::derived_from<Decl> DeclTy>
    void
    populate(SourceInfo& I, DeclTy const* D);

    /*  Parse the comments above a declaration as Javadoc

        This function will parse the comments above a declaration
        as Javadoc, and store the results in the `javadoc` input
        parameter.

        @return true if the comments were successfully parsed as
        Javadoc, and false otherwise.
     */
    bool
    populate(
        std::optional<Javadoc>& javadoc,
        Decl const* D);

    void
    populate(SourceInfo& I, clang::SourceLocation loc, bool definition, bool documented);

    void
    populate(NamespaceInfo& I, NamespaceDecl const* D);

    static
    void
    populate(NamespaceInfo& I, TranslationUnitDecl const* D);

    void
    populate(RecordInfo& I, CXXRecordDecl const* D);

    void
    populate(RecordInfo& I, ClassTemplateDecl const* D);

    void
    populate(RecordInfo& I, ClassTemplateSpecializationDecl const* D);

    void
    populate(RecordInfo& I, ClassTemplatePartialSpecializationDecl const* D);

    void
    populate(FunctionInfo& I, FunctionDecl const* D);

    void
    populate(FunctionInfo& I, FunctionTemplateDecl const* D);

    void
    populate(FunctionInfo& I, CXXMethodDecl const* D);

    void
    populate(FunctionInfo& I, CXXConstructorDecl const* D);

    void
    populate(FunctionInfo& I, CXXDestructorDecl const* D);

    void
    populate(FunctionInfo& I, CXXConversionDecl const* D);

    void
    populate(EnumInfo& I, EnumDecl const* D);

    void
    populate(EnumConstantInfo& I, EnumConstantDecl const* D);

    void
    populate(TypedefInfo& I, TypedefNameDecl const* D);

    void
    populate(TypedefInfo& I, TypedefDecl const* D);

    void
    populate(TypedefInfo& I, TypeAliasDecl const* D);

    void
    populate(TypedefInfo& I, TypeAliasTemplateDecl const* D);

    void
    populate(VariableInfo& I, VarDecl const* D);

    void
    populate(VariableInfo& I, VarTemplateDecl const* D);

    void
    populate(VariableInfo& I, VarTemplateSpecializationDecl const* D);

    void
    populate(VariableInfo& I, VarTemplatePartialSpecializationDecl const* D);

    void
    populate(FieldInfo& I, FieldDecl const* D);

    void
    populate(FriendInfo& I, FriendDecl const* D);

    void
    populate(GuideInfo& I, CXXDeductionGuideDecl const* D);

    void
    populate(GuideInfo& I, FunctionTemplateDecl const* D);

    void
    populate(NamespaceAliasInfo& I, NamespaceAliasDecl const* D);

    void
    populate(UsingInfo& I, UsingDecl const* D);

    void
    populate(ConceptInfo& I, ConceptDecl const* D);

    /*  Default function to populate the template information

        This overload ignores the declaration and populates
        the template information with the template parameters
        of the template declaration.
     */
    template <
        std::derived_from<Decl> DeclTy,
        std::derived_from<TemplateDecl> TemplateDeclTy>
    void
    populate(TemplateInfo& Template, DeclTy const* D, TemplateDeclTy const* TD);

    void
    populate(TemplateInfo& Template, ClassTemplateSpecializationDecl const* D, ClassTemplateDecl const* CTD);

    /*  Populate the template information for a variable template

        The function will populate the template parameters
        depending on whether the variable is a specialization.
    */
    template<std::derived_from<VarDecl> VarDeclTy>
    void
    populate(TemplateInfo& Template, VarDeclTy const* D, VarTemplateDecl const* VTD);

    template<
        std::derived_from<Decl> DeclTy,
        std::derived_from<TemplateDecl> TemplateDeclTy>
    void
    populate(std::optional<TemplateInfo>& Template, DeclTy const* D, TemplateDeclTy const* VTD)
    {
        MRDOCS_CHECK_OR(VTD);
        MRDOCS_CHECK_OR(!VTD->isImplicit());
        if (TemplateParameterList const* TPL = VTD->getTemplateParameters();
            !TPL->empty() &&
            std::ranges::none_of(TPL->asArray(), [](NamedDecl const* ND) {
                return !ND->isImplicit();
            }))
        {
            return;
        }
        if (!Template)
        {
            Template.emplace();
        }
        TemplateInfo &TI = *Template;
        populate(TI, D, VTD);
    }

    void
    populate(NoexceptInfo& I, FunctionProtoType const* FPT);

    void
    populate(ExplicitInfo& I, ExplicitSpecifier const& ES);

    void
    populate(ExprInfo& I, Expr const* E);

    template <class T>
    void
    populate(ConstantExprInfo<T>& I, Expr const* E);

    template <class T>
    void
    populate(ConstantExprInfo<T>& I, Expr const* E, llvm::APInt const& V);

    void
    populate(Polymorphic<TParam>& I, NamedDecl const* N);

    void
    populate(std::optional<TemplateInfo>& TI, TemplateParameterList const* TPL)
    {
        if (!TI)
        {
            TI.emplace();
        }
        populate(*TI, TPL);
    }

    void
    populate(TemplateInfo& TI, TemplateParameterList const* TPL);

    template <range_of<TemplateArgument> Range>
    void
    populate(
        std::vector<Polymorphic<TArg>>& result,
        Range&& args)
    {
        for (TemplateArgument const& arg : args)
        {
            if (arg.getIsDefaulted())
            {
                break;
            }
            // KRYSTIAN NOTE: is this correct? should we have a
            // separate TArgKind for packs instead of "unlaminating"
            // them as we are doing here?
            if (arg.getKind() == TemplateArgument::Pack)
            {
                populate(result, arg.pack_elements());
            }
            else
            {
                result.emplace_back(toTArg(arg));
            }
        }
    }

    void
    populate(
        std::vector<Polymorphic<TArg>>& result,
        ASTTemplateArgumentListInfo const* args);

    template <std::derived_from<Info> InfoTy>
    static
    void
    populateAttributes(InfoTy& I, Decl const* D);

    // =================================================
    // Populate function helpers
    // =================================================
    template <std::derived_from<Decl> DeclTy>
    std::string
    extractName(DeclTy const* D);

    // Extract the name of a declaration
    std::string
    extractName(NamedDecl const* D);

    // Extract the name of a declaration
    std::string
    extractName(DeclarationName N);

    SmallString<256>
    qualifiedName(Decl const* D) const;

    SmallString<256>
    qualifiedName(NamedDecl const* ND) const;

    void
    addMember(NamespaceInfo& I, Info const& Member);

    void
    addMember(RecordInfo& I, Info const& Member);

    void
    addMember(RecordTranche& I, Info const& Member);

    void
    addMember(EnumInfo& I, Info const& Member) const;

    void
    addMember(OverloadsInfo& I, Info const& Member) const;

    void
    addMember(std::vector<SymbolID>& container, Info const& Member) const;

    Polymorphic<TypeInfo>
    toTypeInfo(QualType qt, TraversalMode mode);

    Polymorphic<TypeInfo>
    toTypeInfo(QualType const qt)
    {
        return toTypeInfo(qt, TraversalMode::Dependency);
    }

    Polymorphic<NameInfo>
    toNameInfo(
        NestedNameSpecifier const* NNS);

    template <class TArgRange = ArrayRef<TemplateArgument>>
    Polymorphic<NameInfo>
    toNameInfo(
        DeclarationName Name,
        std::optional<TArgRange> TArgs = std::nullopt,
        NestedNameSpecifier const* NNS = nullptr);

    template <class TArgRange = ArrayRef<TemplateArgument>>
    Polymorphic<NameInfo>
    toNameInfo(
        Decl const* D,
        std::optional<TArgRange> TArgs = std::nullopt,
        NestedNameSpecifier const* NNS = nullptr);

    Polymorphic<TArg>
    toTArg(TemplateArgument const& A);

    // Pretty-print an expression
    std::string
    toString(Expr const* E);

    // Pretty-print a type
    std::string
    toString(Type const* T);

    template <class Integer>
    Integer
    toInteger(llvm::APInt const& V);

    /*  Get the original source code in a range

        This function is used to populate the information in
        Info types that contain expressions as written
        in the source code, such as the value of default
        arguments.
     */
    std::string
    getSourceCode(SourceRange const& R) const;

    /*  Struct to hold the underlying type result of a SFINAE type.

        This struct is used to store the underlying type and the template
        arguments of a type used in a SFINAE context.

        For instance, for the type `std::enable_if_t<std::is_integral_v<T>, T>`,
        the `Type` would be `T`, and the `Arguments` would be
        `{std::is_integral_v<T>, T}`.
     */
    struct SFINAEInfo
    {
        // The underlying type of the SFINAE type.
        QualType Type;

        // The template arguments used in the SFINAE context.
        std::vector<ExprInfo> Constraints;
    };

    /* Get the underlying type result of a SFINAE type

        This function will return the underlying type of a
        type used in a SFINAE context.

        For instance, it returns the type `T` for the
        type `std::enable_if_t<std::is_integral_v<T>, T>`.

        @param T The type to check.

        @return `std::nullopt` if the type is not a SFINAE type,
        and the underlying type with the template arguments
        otherwise.
     */
    std::optional<SFINAEInfo>
    extractSFINAEInfo(QualType T);

    // @copydoc extractSFINAEInfo(QualType)
    std::optional<SFINAEInfo>
    extractSFINAEInfo(Type const* T)
    {
        return extractSFINAEInfo(QualType(T, 0));
    }

    /* Struct to hold SFINAE information.

       This struct is used to store information about a template that is
       involved in a SFINAE context. It contains the template declaration,
       the member identifier, and the template arguments.

       For instance, for the type `std::enabled_if_t<std::is_integral_v<T>, T>`,
       the struct would contain the `Template` declaration for `std::enable_if`,
       and the `Arguments` `{std::is_integral_v<T>, T}`.
     */
    struct SFINAETemplateInfo
    {
        /// The template declaration involved in SFINAE
        TemplateDecl* Template = nullptr;

        /// The identifier of the member being checked.
        IdentifierInfo const* Member = nullptr;

        /// The template arguments used in the SFINAE context.
        ArrayRef<TemplateArgument> Arguments;
    };

    /* Get the template declaration and member identifier

        This function is a helper used by `extractSFINAEInfo` to
        extract the template declaration and member identifier
        from a type used in a SFINAE context.

        For instance, for the type `std::enabled_if_t<std::is_integral_v<T>, T>`,
        the struct would contain the `Template` declaration for `std::enable_if`,
        and the `Arguments` `{std::is_integral_v<T>, T}`.

        If `AllowDependentNames` is set to `true`, the function will
        also populate the `Member` field with the member identifier
        if the type is a dependent name type. For instance,
        for the type `typename std::enable_if<B,T>::type`, the `Member`
        field would be `type`, and the other fields would be populated
        respective to `std::enable_if<B,T>`: `Template` would be `std::enable_if`,
        and `Arguments` would be `{B,T}`.
     */
    std::optional<SFINAETemplateInfo>
    getSFINAETemplateInfo(QualType T, bool AllowDependentNames) const;

    /* The controlling parameters of a SFINAE template

       This struct is used to store information about the controlling
       parameters of a template that is involved in a SFINAE context.

       It contains the template parameters, the controlling parameters,
       and the index of the parameter that represents the result.

       For instance, for the template `template<bool B, typename T> typename
       std::enable_if<B,T>::type`, the struct would contain the template
       parameters `{B, T}`, the index of the controlling parameters `{B}`,
       and the index `1` to represent the second result parameter `T`.
     */
    struct SFINAEControlParams
    {
        // The template parameters of the template declaration
        TemplateParameterList* Parameters = nullptr;

        // The controlling parameters of the template declaration
        llvm::SmallBitVector ControllingParams;

        // The index of the parameter that represents the SFINAE result
        std::size_t ParamIdx = static_cast<std::size_t>(-1);
    };

    /* Determine if a template is SFINAE and returns constraints

       This function is used by `isSFINAETemplate` to determine if a
       template is involved in a SFINAE context.

       If the template is involved in a SFINAE context, the function
       will return the template parameters, the controlling parameters,
       and the index of the parameter that controls the SFINAE context.

       If the template is an alias (such as `std::enable_if_t`), the
       template information of the underlying type
       (such as `typename enable_if<B,T>::type`) will be extract instead.
     */
    std::optional<SFINAEControlParams>
    getSFINAEControlParams(TemplateDecl* TD, IdentifierInfo const* Member);

    std::optional<SFINAEControlParams>
    getSFINAEControlParams(SFINAETemplateInfo const& SFINAE) {
        return getSFINAEControlParams(SFINAE.Template, SFINAE.Member);
    }

    /* Get the template argument with specified index.

        If the index is a valid index in the template arguments,
        the function will return the template argument at the
        specified index.

        If the index is outside the bounds of the
        arguments, the function will attempt to get the
        argument from the default template arguments.
     */
    std::optional<TemplateArgument>
    tryGetTemplateArgument(
        TemplateParameterList* Parameters,
        ArrayRef<TemplateArgument> Arguments,
        std::size_t Index);

    // =================================================
    // Filters
    // =================================================

    /*  Determine in what mode a declaration should be extracted

        This function will determine whether a declaration
        should be extracted and what extraction mode
        applies to the symbol.

        If the symbol should not be extracted, the function
        will return `ExtractionMode::Dependency`, meaning
        we should only extract it if the traversal is in
        dependency mode.

        The function filters private symbols, symbols outside
        the input files, and symbols in files that do not match
        the input file patterns.

        Any configuration options that affect extraction
        will be checked here.

        @param D the declaration to check
        @param access the access specifier of the declaration

        @return Return the most specific extraction mode
        possible for the declaration.
     */
    ExtractionMode
    checkFilters(Decl const* D, AccessSpecifier access);

    static
    ExtractionMode
    checkFilters(TranslationUnitDecl const*, AccessSpecifier)
    {
        return ExtractionMode::Regular;
    }

    template <std::derived_from<Decl> DeclTy>
    ExtractionMode
    checkFilters(DeclTy const* D)
    {
        AccessSpecifier A = getAccess(D);
        return checkFilters(D, A);
    }

    bool
    checkTypeFilters(Decl const* D, AccessSpecifier access);

    bool
    checkFileFilters(Decl const* D);

    bool
    checkFileFilters(std::string_view symbolPath) const;

    /* Check all symbol filters for a declaration

       @param D The declaration to check
       @param AllowParent Whether a member declaration should
       be allowed to inherit the value from the parent.
     */
    ExtractionInfo
    checkSymbolFilters(Decl const* D, bool AllowParent);

    ExtractionInfo
    checkSymbolFilters(Decl const* D)
    {
        return checkSymbolFilters(D, true);
    }

    /* The strategy for checking filters in `checkSymbolFilters`
     */
    enum SymbolCheckType {
        // Check if the symbol matches one of the patterns as a whole
        Strict,
        // Check if the symbol matches the prefix of the patterns
        // even if the symbol is not a whole match
        PrefixOnly,
        // Check if the pattern is literal and the symbol contains
        // exactly the same contents.
        Literal
    };

    /* Check if the symbol name matches one of the patterns

        This function checks if the symbol name matches one of the
        patterns according to the strategy defined by `t`.
     */
    template <SymbolCheckType t>
    bool
    checkSymbolFiltersImpl(
        std::vector<SymbolGlobPattern> const& patterns,
        std::string_view symbolName) const;


    // =================================================
    // Element access
    // =================================================

    /*  Get Info from ASTVisitor InfoSet
     */
    Info*
    find(SymbolID const& id) const;

     /*  Get Info from ASTVisitor InfoSet

        This function will generate a symbol ID for the
        declaration and call the other `find` function
        to get the Info object for the declaration.
     */
    Info*
    find(Decl const* D) const;

    /*  Find or traverse a declaration

        This function will first attempt to find the Info
        object for a declaration in the InfoSet.

        If the Info object does not exist, the function
        will traverse the declaration and create a new
        Info object for the declaration.

        This function is useful when we need to find the
        Info object for a parameter or dependency, but
        we don't want to traverse or populate the
        declaration if it has already been extracted
        because the context where the symbol is being
        used (such as function parameters or aliases),
        does not contain extra non-redundant and relevant
        information about the symbol.

        Thus, it avoids circular dependencies and infinite
        loops when traversing the AST. If the traversal
        has been triggered by a dependency, the second
        call to this function will return the Info object
        that was created during the first call.

        @param D The declaration to find or traverse.

        @return a pointer to the Info object.
     */
    Info*
    findOrTraverse(Decl const* D)
    {
        MRDOCS_CHECK_OR(D, nullptr);
        if (auto* I = find(D))
        {
            return I;
        }
        return traverse(D);
    }

    /* Get FileInfo from ASTVisitor files

        This function will return a pointer to a FileInfo
        object for a given Clang FileEntry object.

        If the FileInfo object does not exist in the cache,
        the function will build a new FileInfo object and
        add it to the files_ map.

        @param file The Clang FileEntry object to get the FileInfo for.

        @return a pointer to the FileInfo object.
     */
    FileInfo*
    findFileInfo(clang::SourceLocation loc);

    FileInfo*
    findFileInfo(Decl const* D);

    /* Build a FileInfo for a string path

        This function will build a FileInfo object for a
        given file path.

        The function will extract the full path and short
        path of the file, and store the information in the
        FileInfo object.

        The search paths will be used to identify the
        short path of the file relative to the search
        directories.

        This function is used as an auxiliary function to
        `buildFileInfo` once the file path has been extracted
        from the FileEntry object.

        @param file_path The file path to build the FileInfo for.
        @return the FileInfo object.
     */
    FileInfo
    buildFileInfo(std::string_view path);

    /* Result of an upsert operation

        This struct is used to return the result of an
        upsert operation. The struct contains a reference
        to the Info object that was created or found, and
        a boolean flag indicating whether the Info object
        was newly created or not.
     */
    template <class InfoTy>
    struct upsertResult {
        InfoTy& I;
        bool isNew;
    };

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
    template <
        class InfoTy = void,
        HasInfoTypeFor DeclType>
    Expected<
        upsertResult<
            std::conditional_t<
                std::same_as<InfoTy, void>,
                InfoTypeFor_t<DeclType>,
                InfoTy>>>
    upsert(DeclType const* D);

    template <
        std::derived_from<Info> InfoTy,
        std::derived_from<Decl> DeclTy>
    Expected<void>
    checkUndocumented(
        SymbolID const& id,
        DeclTy const* D);
};

} // clang::mrdocs

#endif
