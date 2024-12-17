//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "lib/AST/ASTVisitor.hpp"
#include "lib/AST/NameInfoBuilder.hpp"
#include "lib/AST/ClangHelpers.hpp"
#include "lib/AST/ParseJavadoc.hpp"
#include "lib/AST/TypeInfoBuilder.hpp"
#include "lib/Support/Path.hpp"
#include "lib/Support/Debug.hpp"
#include "lib/Support/Glob.hpp"
#include "lib/Lib/Diagnostics.hpp"
#include "lib/Lib/Filters.hpp"
#include "lib/AST/SymbolFilterScope.hpp"
#include <mrdocs/Metadata.hpp>
#include <clang/AST/AST.h>
#include <clang/AST/Attr.h>
#include <clang/AST/ODRHash.h>
#include <clang/AST/TypeVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Index/USRGeneration.h>
#include <clang/Lex/Lexer.h>
#include <clang/Sema/Lookup.h>
#include <clang/Sema/Sema.h>
#include <clang/Sema/Template.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/SHA1.h>
#include <memory>
#include <optional>
#include <ranges>
#include <unordered_set>

namespace clang::mrdocs {

auto
ASTVisitor::FileInfo::build(
    std::span<std::pair<std::string, FileKind> const> search_dirs,
    std::string_view const file_path,
    std::string_view const sourceRoot) -> ASTVisitor::FileInfo {
    FileInfo file_info;
    file_info.full_path = std::string(file_path);
    file_info.short_path = file_info.full_path;

    std::ptrdiff_t best_length = 0;
    auto check_dir = [&](
        std::string_view const dir_path,
        FileKind const kind)
    {
        auto NI = llvm::sys::path::begin(file_path);
        auto const NE = llvm::sys::path::end(file_path);
        auto DI = llvm::sys::path::begin(dir_path);
        auto const DE = llvm::sys::path::end(dir_path);

        for(; NI != NE; ++NI, ++DI)
        {
            // reached the end of the directory path
            if(DI == DE)
            {
                // update the best prefix length
                if(std::ptrdiff_t length =
                    NI - llvm::sys::path::begin(file_path);
                    length > best_length)
                {
                    best_length = length;
                    file_info.kind = kind;
                    return true;
                }
                break;
            }
            // separators always match
            if(NI->size() == 1 && DI->size() == 1 &&
                llvm::sys::path::is_separator(NI->front()) &&
                llvm::sys::path::is_separator(DI->front()))
                continue;
            // components don't match
            if(*NI != *DI)
                break;
        }
        return false;
    };

    bool const in_sourceRoot = check_dir(
        sourceRoot, FileKind::Source);

    // only use a sourceRoot relative path if we
    // don't find anything in the include search directories
    // bool any_match = false;
    // for (auto const& [dir_path, kind]: search_dirs)
    // {
    //    any_match |= check_dir(dir_path, kind);
    // }

    // KRYSTIAN TODO: if we don't find any matches,
    // make the path relative to sourceRoot and return it

    // override the file kind if
    // the file was found in sourceRoot
    if (in_sourceRoot)
    {
        file_info.kind = FileKind::Source;
    }

    file_info.short_path.erase(0, best_length);
    return file_info;
}

ASTVisitor::
ASTVisitor(
    const ConfigImpl& config,
    Diagnostics& diags,
    CompilerInstance& compiler,
    ASTContext& context,
    Sema& sema) noexcept
    : config_(config)
    , diags_(diags)
    , compiler_(compiler)
    , context_(context)
    , source_(context.getSourceManager())
    , sema_(sema)
    , symbolFilter_(config->symbolFilter)
{
    // Install handlers for our custom commands
    initCustomCommentCommands(context_);

    // The traversal scope should *only* consist of the
    // top-level TranslationUnitDecl.
    // If this `assert` fires, then it means
    // ASTContext::setTraversalScope is being (erroneously)
    // used somewhere
    MRDOCS_ASSERT(context_.getTraversalScope() ==
        std::vector<Decl*>{context_.getTranslationUnitDecl()});

    auto make_posix_absolute = [&](std::string_view const old_path)
    {
        llvm::SmallString<128> new_path(old_path);
        if(! llvm::sys::path::is_absolute(new_path))
        {
            // KRYSTIAN FIXME: use FileManager::makeAbsolutePath?
            auto const& cwd = source_.getFileManager().
                getFileSystemOpts().WorkingDir;
            // we can't normalize a relative path
            // without a base directory
            // MRDOCS_ASSERT(! cwd.empty());
            llvm::sys::fs::make_absolute(cwd, new_path);
        }
        // remove ./ and ../
        llvm::sys::path::remove_dots(new_path, true, llvm::sys::path::Style::posix);
        // convert to posix style
        llvm::sys::path::native(new_path, llvm::sys::path::Style::posix);
        return std::string(new_path);
    };

    Preprocessor& PP = sema_.getPreprocessor();
    HeaderSearch& HS = PP.getHeaderSearchInfo();
    std::vector<std::pair<std::string, FileKind>> search_dirs;
    search_dirs.reserve(HS.search_dir_size());
    // first, convert all the include search directories into POSIX style
    for (const DirectoryLookup& DL : HS.search_dir_range())
    {
        OptionalDirectoryEntryRef DR = DL.getDirRef();
        // only consider normal directories
        if(! DL.isNormalDir() || ! DR)
            continue;
        // store the normalized path
        search_dirs.emplace_back(
            make_posix_absolute(DR->getName()),
            DL.isSystemHeaderDirectory() ?
                FileKind::System : FileKind::Other);
    }

    std::string const sourceRoot = make_posix_absolute(config_->sourceRoot);
    auto cacheFileInfo = [&](const FileEntry* entry)
    {
        // "try" implies this may fail, so fallback to getName
        // if an empty string is returned
        std::string_view file_path =
            entry->tryGetRealPathName();
        files_.emplace(
            entry,
            FileInfo::build(
                search_dirs,
                make_posix_absolute(file_path),
                sourceRoot));
    };

    // build the file info for the main file
    cacheFileInfo(source_.getFileEntryForID(source_.getMainFileID()));

    // build the file info for all included files
    for(const FileEntry* file : PP.getIncludedFiles())
    {
        cacheFileInfo(file);
    }
}

void
ASTVisitor::
build()
{
    // traverse the translation unit, only extracting
    // declarations which satisfy all filter conditions.
    // dependencies will be tracked, but not extracted
    traverseAny(context_.getTranslationUnitDecl());

    // Ensure the global namespace is always present
    upsert<NamespaceInfo>(SymbolID::global);

    // Dependency extraction is disabled: we are done
    if(config_->referencedDeclarations ==
        ConfigImpl::SettingsImpl::ExtractPolicy::Never) {
        return;
    }
    buildDependencies();
}

void
ASTVisitor::
buildDependencies()
{
    auto scope = enterMode(ExtractMode::DirectDependency);
    std::unordered_set<Decl*> currentPass(std::move(dependencies_));
    while (!currentPass.empty())
    {
        for (Decl* D : currentPass)
        {
            SymbolID id = generateID(D);
            bool const invalidId = !id;
            bool const alreadyExtracted = info_.contains(id);
            if (invalidId || alreadyExtracted)
            {
                continue;
            }
            traverseAny(D);
        }
        currentPass = std::move(dependencies_);
    }
}

namespace {
template <class DeclTy>
concept isRedeclarableTemplate =
    std::derived_from<DeclTy, RedeclarableTemplateDecl>;

template <class DeclTy>
concept isTemplateSpecialization =
    std::derived_from<DeclTy, ClassTemplateSpecializationDecl> ||
    std::derived_from<DeclTy, VarTemplateSpecializationDecl>;
} // (anon)


void
ASTVisitor::
traverseAny(Decl* D)
{
    MRDOCS_ASSERT(D);
    MRDOCS_CHECK_OR(!D->isInvalidDecl());

    // Restore the symbol filter state when leaving the scope
    SymbolFilterScope scope(symbolFilter_);

    // Convert to the most derived type of the Decl
    // and call the appropriate traverse function
    visit(D, [&]<typename DeclTy>(DeclTy* DD)
    {
        if constexpr (isRedeclarableTemplate<DeclTy>)
        {
            // If the symbol is a template, we traverse the
            // templated declaration instead of the template.
            // The template is provided as the second argument
            // so that the Info object can also be populated with
            // this information.
            traverseAny(DD->getTemplatedDecl(), DD);
        }
        else if constexpr (isTemplateSpecialization<DeclTy>)
        {
            // If the symbol is a template specialization,
            // we traverse the specialized template and provide
            // the specialized template as the second argument.
            traverse(DD, DD->getSpecializedTemplate());
        }
        else
        {
            // In the general case, we call the appropriate
            // traverse function for the most derived type
            // of the Decl
            traverse(DD);
        }
    });
}

template <std::derived_from<TemplateDecl> TemplateDeclTy>
void
ASTVisitor::
traverseAny(Decl* D, TemplateDeclTy* TD)
{
    MRDOCS_ASSERT(D);
    MRDOCS_ASSERT(TD);
    MRDOCS_CHECK_OR(!D->isInvalidDecl());
    MRDOCS_CHECK_OR(!TD->isInvalidDecl());

    // Restore the symbol filter state when leaving the scope
    SymbolFilterScope scope(symbolFilter_);

    // Convert to the most derived type of the Decl
    // and call the appropriate traverse function
    visit(D, [&]<typename DeclTy>(DeclTy* DD)
    {
        if constexpr (
            isRedeclarableTemplate<DeclTy> ||
            isTemplateSpecialization<DeclTy>)
        {
            // If `D` is still a template or a template specialization,
            // we traverse the secondary templated declaration instead.
            traverseAny(DD);
        }
        else
        {
            // Call the appropriate traverse function
            // for the most derived type of the Decl.
            // The primary template information provided to
            // the corresponding traverse function.
            traverse(DD, TD);
        }
    });
}

template <
    bool PopulateFromTD,
    std::derived_from<Decl> DeclTy,
    std::derived_from<TemplateDecl> TemplateDeclTy>
void
ASTVisitor::
defaultTraverseImpl(DeclTy* D, TemplateDeclTy* TD)
{
    MRDOCS_ASSERT(D);
    MRDOCS_ASSERT(!PopulateFromTD || TD);

    // If D is also a template declaration, we extract
    // the template information from the declaration itself
    if constexpr (!HasInfoTypeFor<DeclTy>)
    {
        // Traverse the members of the declaration even if
        // the declaration does not have a corresponding Info type
        traverseMembers(D);
    }
    else
    {
        if constexpr (!PopulateFromTD && std::derived_from<DeclTy, TemplateDecl>)
        {
            defaultTraverseImpl<true>(D, D);
            return;
        }

        // If the declaration has a corresponding Info type,
        // we build the Info object and populate it with the
        // necessary information.
        auto exp = upsert(D);
        MRDOCS_CHECK_OR(exp);

        auto& [I, isNew] = *exp;

        // Populate template information
        if constexpr (
            PopulateFromTD &&
            requires { I.Template; })
        {
            if (!I.Template)
            {
                I.Template.emplace();
            }
            populate(*I.Template, D, TD);
        }

        // Populate the Info object with the necessary information
        populate(I, isNew, D);

        // Traverse the members of the declaration
        traverseMembers(D);
    }
}

template<class FunctionDeclTy>
requires
    std::derived_from<FunctionDeclTy, Decl> &&
    std::derived_from<FunctionDeclTy, FunctionDecl>
void
ASTVisitor::
traverse(FunctionDeclTy* D)
{
    auto exp = upsert(D);
    if (!exp)
    {
        return;
    }
    auto [I, isNew] = *exp;

    // D is the templated declaration if FTD is non-null
    if (D->isFunctionTemplateSpecialization())
    {
        if (!I.Template)
        {
            I.Template.emplace();
        }

        if (auto* FTSI = D->getTemplateSpecializationInfo())
        {
            generateID(getInstantiatedFrom(
                FTSI->getTemplate()), I.Template->Primary);

            // TemplateArguments is used instead of TemplateArgumentsAsWritten
            // because explicit specializations of function templates may have
            // template arguments deduced from their return type and parameters
            if(auto* Args = FTSI->TemplateArguments)
            {
                populate(I.Template->Args, Args->asArray());
            }
        }
        else if (auto* DFTSI = D->getDependentSpecializationInfo())
        {
            // Only extract the ID of the primary template if there is
            // a single candidate primary template.
            if (auto Candidates = DFTSI->getCandidates(); Candidates.size() == 1)
            {
                generateID(getInstantiatedFrom(
                    Candidates.front()), I.Template->Primary);
            }
            if(auto* Args = DFTSI->TemplateArgumentsAsWritten)
            {
                populate(I.Template->Args, Args);
            }
        }
    }

    populate(I, isNew, D);
}

void
ASTVisitor::
traverse(UsingDirectiveDecl* D)
{
    // A using directive such as `using namespace std;`
    if (!shouldExtract(D, getAccess(D)))
    {
        return;
    }

    Decl* PD = getParentDecl(D);
    bool const isNamespaceScope = cast<DeclContext>(PD)->isFileContext();
    MRDOCS_CHECK_OR(isNamespaceScope);

    if (Info* PI = find(generateID(PD)))
    {
        MRDOCS_ASSERT(PI->isNamespace());
        auto* NI = dynamic_cast<NamespaceInfo*>(PI);
        upsertDependency(
            D->getNominatedNamespaceAsWritten(),
            NI->UsingDirectives.emplace_back());
    }
}

void
ASTVisitor::
traverse(IndirectFieldDecl* D)
{
    traverse(D->getAnonField());
}

template <std::derived_from<Decl> DeclTy>
void
ASTVisitor::
traverseMembers(DeclTy* DC)
{
    // When a declaration context is a function, we should
    // not traverse its members as function arguments are
    // not main Info members.
    if constexpr (
        !std::derived_from<DeclTy, FunctionDecl> &&
        std::derived_from<DeclTy, DeclContext>)
    {
        for (auto* D : DC->decls())
        {
            traverseAny(D);
        }
    }
}

Expected<llvm::SmallString<128>>
ASTVisitor::
generateUSR(const Decl* D) const
{
    MRDOCS_ASSERT(D);
    llvm::SmallString<128> res;

    if (auto const* NAD = dyn_cast<NamespaceAliasDecl>(D))
    {
        if (index::generateUSRForDecl(cast<Decl>(NAD->getNamespace()), res))
        {
            return Unexpected(Error("Failed to generate USR"));
        }
        res.append("@NA");
        res.append(NAD->getNameAsString());
        return res;
    }

    // Handling UsingDecl
    if (auto const* UD = dyn_cast<UsingDecl>(D))
    {
        for (const auto* shadow : UD->shadows())
        {
            if (index::generateUSRForDecl(shadow->getTargetDecl(), res))
            {
                return Unexpected(Error("Failed to generate USR"));
            }
        }
        res.append("@UDec");
        res.append(UD->getQualifiedNameAsString());
        return res;
    }

    // Handling UnresolvedUsingTypenameDecl
    if (auto const* UD = dyn_cast<UnresolvedUsingTypenameDecl>(D))
    {
        if (index::generateUSRForDecl(UD, res))
        {
            return Unexpected(Error("Failed to generate USR"));
        }
        res.append("@UUTDec");
        res.append(UD->getQualifiedNameAsString());
        return res;
    }

    // Handling UnresolvedUsingValueDecl
    if (auto const* UD = dyn_cast<UnresolvedUsingValueDecl>(D))
    {
        if (index::generateUSRForDecl(UD, res))
        {
            return Unexpected(Error("Failed to generate USR"));
        }
        res.append("@UUV");
        res.append(UD->getQualifiedNameAsString());
        return res;
    }

    // Handling UsingPackDecl
    if (auto const* UD = dyn_cast<UsingPackDecl>(D))
    {
        if (index::generateUSRForDecl(UD, res))
        {
            return Unexpected(Error("Failed to generate USR"));
        }
        res.append("@UPD");
        res.append(UD->getQualifiedNameAsString());
        return res;
    }

    // Handling UsingEnumDecl
    if (auto const* UD = dyn_cast<UsingEnumDecl>(D))
    {
        if (index::generateUSRForDecl(UD, res))
        {
            return Unexpected(Error("Failed to generate USR"));
        }
        res.append("@UED");
        EnumDecl const* ED = UD->getEnumDecl();
        res.append(ED->getQualifiedNameAsString());
        return res;
    }

    // KRYSTIAN NOTE: clang doesn't currently support
    // generating USRs for friend declarations, so we
    // will improvise until I can merge a patch which
    // adds support for them
    if(auto const* FD = dyn_cast<FriendDecl>(D))
    {
        // first, generate the USR for the containing class
        if (index::generateUSRForDecl(cast<Decl>(FD->getDeclContext()), res))
        {
            return Unexpected(Error("Failed to generate USR"));
        }
        // add a seperator for uniqueness
        res.append("@FD");
        // if the friend declaration names a type,
        // use the USR generator for types
        if (TypeSourceInfo* TSI = FD->getFriendType())
        {
            if (index::generateUSRForType(TSI->getType(), context_, res))
            {
                return Unexpected(Error("Failed to generate USR"));
            }
            return res;
        }
        // otherwise, fallthrough and append the
        // USR of the nominated declaration
        if (!((D = FD->getFriendDecl())))
        {
            return Unexpected(Error("Failed to generate USR"));
        }
    }

    if (index::generateUSRForDecl(D, res))
        return Unexpected(Error("Failed to generate USR"));

    const auto* Described = dyn_cast_if_present<TemplateDecl>(D);
    const auto* Templated = D;
    if (auto const* DT = D->getDescribedTemplate())
    {
        Described = DT;
        if (auto const* TD = DT->getTemplatedDecl())
        {
            Templated = TD;
        }
    }

    if(Described)
    {
        const TemplateParameterList* TPL = Described->getTemplateParameters();
        if(const auto* RC = TPL->getRequiresClause())
        {
            RC = SubstituteConstraintExpressionWithoutSatisfaction(
                sema_, cast<NamedDecl>(isa<FunctionTemplateDecl>(Described) ? Described : Templated), RC);
            if (!RC)
            {
                return Unexpected(Error("Failed to generate USR"));
            }
            ODRHash odr_hash;
            odr_hash.AddStmt(RC);
            res.append("@TPL#");
            res.append(llvm::itostr(odr_hash.CalculateHash()));
        }
    }

    if(auto* FD = dyn_cast<FunctionDecl>(Templated);
        FD && FD->getTrailingRequiresClause())
    {
        const Expr* RC = FD->getTrailingRequiresClause();
        RC = SubstituteConstraintExpressionWithoutSatisfaction(
            sema_, cast<NamedDecl>(Described ? Described : Templated), RC);
        if (!RC)
        {
            return Unexpected(Error("Failed to generate USR"));
        }
        ODRHash odr_hash;
        odr_hash.AddStmt(RC);
        res.append("@TRC#");
        res.append(llvm::itostr(odr_hash.CalculateHash()));
    }

    return res;
}

bool
ASTVisitor::
generateID(
    const Decl* D,
    SymbolID& id) const
{
    if (!D)
    {
        return false;
    }

    if (isa<TranslationUnitDecl>(D))
    {
        id = SymbolID::global;
        return true;
    }

    if (auto exp = generateUSR(D))
    {
        auto h = llvm::SHA1::hash(arrayRefFromStringRef(*exp));
        id = SymbolID(h.data());
        return true;
    }

    return false;
}

SymbolID
ASTVisitor::
generateID(const Decl* D) const
{
    SymbolID id = SymbolID::invalid;
    generateID(D, id);
    return id;
}

void
ASTVisitor::
populate(
    NamespaceInfo& I,
    bool const isNew,
    NamespaceDecl* D)
{
    // Only extract Namespace data once
    MRDOCS_CHECK_OR(isNew);
    I.IsAnonymous = D->isAnonymousNamespace();
    if (!I.IsAnonymous)
    {
        I.Name = extractName(D);
    }
    I.IsInline = D->isInline();
    populateNamespaces(I, D);
}

void
ASTVisitor::
populate(
    RecordInfo& I,
    bool const isNew,
    CXXRecordDecl* D)
{
    bool const documented = generateJavadoc(I.javadoc, D);
    populate(I, D->getBeginLoc(),
        D->isThisDeclarationADefinition(), documented);

    if (!isNew)
    {
        return;
    }

    NamedDecl const* ND = D;
    if (TypedefNameDecl const* TD = D->getTypedefNameForAnonDecl())
    {
        I.IsTypeDef = true;
        ND = TD;
    }
    I.Name = extractName(ND);

    I.KeyKind = toRecordKeyKind(D->getTagKind());

    // These are from CXXRecordDecl::isEffectivelyFinal()
    I.IsFinal = D->hasAttr<FinalAttr>();
    if (auto const* DT = D->getDestructor())
    {
        I.IsFinalDestructor = DT->hasAttr<FinalAttr>();
    }

    // Extract direct bases. D->bases() will get the bases
    // from whichever declaration is the definition (if any)
    if(D->hasDefinition())
    {
        for(const CXXBaseSpecifier& B : D->bases())
        {
            AccessSpecifier const access = B.getAccessSpecifier();
            // KRYSTIAN FIXME: we need finer-grained control
            // for protected bases, since an inheriting class
            // will have access to the bases public members...
            if(config_->inaccessibleBases !=
                ConfigImpl::SettingsImpl::ExtractPolicy::Always)
            {
                if(access == AccessSpecifier::AS_private ||
                    access == AccessSpecifier::AS_protected)
                    continue;
            }
            // the extraction of the base type is
            // performed in direct dependency mode
            auto BaseType = toTypeInfo(
                B.getType(),
                ExtractMode::DirectDependency);
            // CXXBaseSpecifier::getEllipsisLoc indicates whether the
            // base was a pack expansion; a PackExpansionType is not built
            // for base-specifiers
            if(BaseType && B.getEllipsisLoc().isValid())
                BaseType->IsPackExpansion = true;
            I.Bases.emplace_back(
                std::move(BaseType),
                convertToAccessKind(access),
                B.isVirtual());
        }
    }

    populateNamespaces(I, D);
}

template <std::derived_from<FunctionDecl> DeclTy>
void
ASTVisitor::
populate(
    FunctionInfo& I,
    bool const isNew,
    DeclTy* D)
{
    bool const documented = generateJavadoc(I.javadoc, D);
    populate(I, D->getBeginLoc(),
        D->isThisDeclarationADefinition(), documented);

    // KRYSTIAN TODO: move other extraction that requires
    // a valid function type here
    if (auto FT = getDeclaratorType(D); ! FT.isNull())
    {
        const auto* FPT = FT->template getAs<FunctionProtoType>();
        populate(I.Noexcept, FPT);
        I.HasTrailingReturn |= FPT->hasTrailingReturn();
    }

    //
    // FunctionDecl
    //
    I.OverloadedOperator = convertToOperatorKind(
        D->getOverloadedOperator());
    I.IsVariadic |= D->isVariadic();
    I.IsDefaulted |= D->isDefaulted();
    I.IsExplicitlyDefaulted |= D->isExplicitlyDefaulted();
    I.IsDeleted |= D->isDeleted();
    I.IsDeletedAsWritten |= D->isDeletedAsWritten();
    I.IsNoReturn |= D->isNoReturn();
        // subsumes D->hasAttr<NoReturnAttr>()
        // subsumes D->hasAttr<CXX11NoReturnAttr>()
        // subsumes D->hasAttr<C11NoReturnAttr>()
        // subsumes D->getType()->getAs<FunctionType>()->getNoReturnAttr()
    I.HasOverrideAttr |= D->template hasAttr<OverrideAttr>();

    if (ConstexprSpecKind const CSK = D->getConstexprKind();
        CSK != ConstexprSpecKind::Unspecified)
    {
        I.Constexpr = toConstexprKind(CSK);
    }

    if (StorageClass const SC = D->getStorageClass())
    {
        I.StorageClass = toStorageClassKind(SC);
    }

    I.IsNodiscard |= D->template hasAttr<WarnUnusedResultAttr>();
    I.IsExplicitObjectMemberFunction |= D->hasCXXExplicitFunctionObjectParameter();
    //
    // CXXMethodDecl
    //
    if constexpr(std::derived_from<DeclTy, CXXMethodDecl>)
    {
        I.IsVirtual |= D->isVirtual();
        I.IsVirtualAsWritten |= D->isVirtualAsWritten();
        I.IsPure |= D->isPureVirtual();
        I.IsConst |= D->isConst();
        I.IsVolatile |= D->isVolatile();
        I.RefQualifier = convertToReferenceKind(D->getRefQualifier());
        I.IsFinal |= D->template hasAttr<FinalAttr>();
        //D->isCopyAssignmentOperator()
        //D->isMoveAssignmentOperator()
        //D->isOverloadedOperator();
        //D->isStaticOverloadedOperator();
    }

    //
    // CXXDestructorDecl
    //
    // if constexpr(std::derived_from<DeclTy, CXXDestructorDecl>)
    // {
    // }

    //
    // CXXConstructorDecl
    //
    if constexpr(std::derived_from<DeclTy, CXXConstructorDecl>)
    {
        populate(I.Explicit, D->getExplicitSpecifier());
    }

    //
    // CXXConversionDecl
    //
    if constexpr(std::derived_from<DeclTy, CXXConversionDecl>)
    {
        populate(I.Explicit, D->getExplicitSpecifier());
    }

    for (ParmVarDecl const* P : D->parameters())
    {
        Param& param = isNew ?
            I.Params.emplace_back() :
            I.Params[P->getFunctionScopeIndex()];

        if (param.Name.empty())
        {
            param.Name = P->getName();
        }

        if (!param.Type)
        {
            param.Type = toTypeInfo(P->getOriginalType());
        }

        const Expr* default_arg = P->hasUninstantiatedDefaultArg() ?
            P->getUninstantiatedDefaultArg() : P->getInit();
        if (param.Default.empty() &&
            default_arg)
        {
            param.Default = getSourceCode(default_arg->getSourceRange());
        }
    }

    if (!isNew)
    {
        return;
    }

    I.Name = extractName(D);

    I.Class = toFunctionClass(D->getDeclKind());

    QualType const RT = D->getReturnType();
    auto next_mode = ExtractMode::IndirectDependency;
    if (auto const* AT = RT->getContainedAutoType();
        AT && AT->hasUnnamedOrLocalType())
    {
        next_mode = ExtractMode::DirectDependency;
    }
    // extract the return type in direct dependency mode
    // if it contains a placeholder type which is
    // deduceded as a local class type
    I.ReturnType = toTypeInfo(RT, next_mode);

    if(auto* TRC = D->getTrailingRequiresClause())
        populate(I.Requires, TRC);

    populateNamespaces(I, D);
}

void
ASTVisitor::
populate(
    EnumInfo& I,
    bool const isNew,
    EnumDecl* D)
{
    bool const documented = generateJavadoc(I.javadoc, D);
    populate(I, D->getBeginLoc(),
        D->isThisDeclarationADefinition(), documented);

    if (!isNew)
    {
        return;
    }

    I.Name = extractName(D);

    I.Scoped = D->isScoped();

    if (D->isFixed())
    {
        I.UnderlyingType = toTypeInfo(D->getIntegerType());
    }

    populateNamespaces(I, D);
}

void
ASTVisitor::
populate(
    EnumConstantInfo& I,
    bool const isNew,
    EnumConstantDecl* D)
{
    bool const documented = generateJavadoc(I.javadoc, D);
    populate(I, D->getBeginLoc(), true, documented);

    if (!isNew)
    {
        return;
    }

    I.Name = extractName(D);

    populate(
        I.Initializer,
        D->getInitExpr(),
        D->getInitVal());

    populateNamespaces(I, D);
}

template<std::derived_from<TypedefNameDecl> TypedefNameDeclTy>
void
ASTVisitor::
populate(
    TypedefInfo& I,
    bool const isNew,
    TypedefNameDeclTy* D)
{
    bool const documented = generateJavadoc(I.javadoc, D);

    // KRYSTIAN FIXME: we currently treat typedef/alias
    // declarations as having a single definition; however,
    // such declarations are never definitions and can
    // be redeclared multiple times (even in the same scope)
    populate(I, D->getBeginLoc(), true, documented);

    if (!isNew)
    {
        return;
    }

    I.IsUsing = isa<TypeAliasDecl>(D);
    I.Name = extractName(D);

    // When a symbol has a dependency on a typedef, we also
    // consider the symbol to have a dependency on the aliased
    // type. Therefore, we propagate the current dependency mode
    // when building the TypeInfo for the aliased type
    I.Type = toTypeInfo(
        D->getUnderlyingType(),
        currentMode());

    populateNamespaces(I, D);
}

void
ASTVisitor::
populate(
    VariableInfo& I,
    bool const isNew,
    VarDecl* D)
{
    bool const documented = generateJavadoc(I.javadoc, D);
    populate(I, D->getBeginLoc(),
        D->isThisDeclarationADefinition(), documented);

    // KRYSTIAN FIXME: we need to properly merge storage class
    if (StorageClass const SC = D->getStorageClass())
    {
        I.StorageClass = toStorageClassKind(SC);
    }

    // this handles thread_local, as well as the C
    // __thread and __Thread_local specifiers
    I.IsThreadLocal |= D->getTSCSpec() !=
        ThreadStorageClassSpecifier::TSCS_unspecified;

    // KRYSTIAN NOTE: VarDecl does not provide getConstexprKind,
    // nor does it use getConstexprKind to store whether
    // a variable is constexpr/constinit. Although
    // only one is permitted in a variable declaration,
    // it is possible to declare a static data member
    // as both constexpr and constinit in separate declarations..
    I.IsConstinit |= D->hasAttr<ConstInitAttr>();
    if (D->isConstexpr())
    {
        I.Constexpr = ConstexprKind::Constexpr;
    }

    if (Expr const* E = D->getInit())
    {
        populate(I.Initializer, E);
    }

    if (!isNew)
    {
        return;
    }

    I.Name = extractName(D);

    I.Type = toTypeInfo(D->getType());

    populateNamespaces(I, D);
}

void
ASTVisitor::
populate(
    FieldInfo& I,
    bool const isNew,
    FieldDecl* D)
{
    bool const documented = generateJavadoc(I.javadoc, D);
    // fields (i.e. non-static data members)
    // cannot have multiple declarations
    populate(I, D->getBeginLoc(), true, documented);

    if (!isNew)
    {
        return;
    }

    I.Name = extractName(D);

    I.Type = toTypeInfo(D->getType());

    I.IsVariant = D->getParent()->isUnion();

    I.IsMutable = D->isMutable();

    if(const Expr* E = D->getInClassInitializer())
        populate(I.Default, E);

    if(D->isBitField())
    {
        I.IsBitfield = true;
        populate(
            I.BitfieldWidth,
            D->getBitWidth());
    }

    I.HasNoUniqueAddress = D->hasAttr<NoUniqueAddressAttr>();
    I.IsDeprecated = D->hasAttr<DeprecatedAttr>();
    I.IsMaybeUnused = D->hasAttr<UnusedAttr>();

    populateNamespaces(I, D);
}

void
ASTVisitor::
populate(
    SpecializationInfo& I,
    bool const isNew,
    ClassTemplateSpecializationDecl* D)
{
    if (!isNew)
    {
        return;
    }

    CXXRecordDecl const* PD = getInstantiatedFrom(D);

    populate(I.Args, D->getTemplateArgs().asArray());

    generateID(PD, I.Primary);
    I.Name = extractName(PD);

    populateNamespaces(I, D);
}

void
ASTVisitor::
populate(
    FriendInfo& I,
    bool const isNew,
    FriendDecl* D)
{
    bool const documented = generateJavadoc(I.javadoc, D);
    populate(I, D->getBeginLoc(), true, documented);

    if (!isNew)
    {
        return;
    }

    // A NamedDecl nominated by a FriendDecl
    // will be one of the following:
    // - FunctionDecl
    // - FunctionTemplateDecl
    // - ClassTemplateDecl
    if (NamedDecl* ND = D->getFriendDecl())
    {
        generateID(ND, I.FriendSymbol);
        // If this is a friend function declaration naming
        // a previously undeclared function, traverse it.
        // in addition to this, traverse the declaration if
        // it's a class templates first declared as a friend
        if((ND->isFunctionOrFunctionTemplate() &&
            ND->getFriendObjectKind() == Decl::FOK_Undeclared) ||
            (isa<ClassTemplateDecl>(ND) && ND->isFirstDecl()))
        {
            traverseAny(ND);
        }
    }

    // Since a friend declaration which name non-class types
    // will be ignored, a type nominated by a FriendDecl can
    // essentially be anything
    if (TypeSourceInfo const* TSI = D->getFriendType())
    {
        I.FriendType = toTypeInfo(TSI->getType());
    }

    populateNamespaces(I, D);
}

void
ASTVisitor::
populate(
    GuideInfo& I,
    bool const isNew,
    CXXDeductionGuideDecl* D)
{
    bool const documented = generateJavadoc(I.javadoc, D);
    populate(I, D->getBeginLoc(), true, documented);

    // deduction guides cannot be redeclared, so there is nothing to merge
    if (!isNew)
    {
        return;
    }

    I.Name = extractName(D->getDeducedTemplate());

    I.Deduced = toTypeInfo(D->getReturnType());

    for (const ParmVarDecl* P : D->parameters())
    {
        I.Params.emplace_back(
            toTypeInfo(P->getOriginalType()),
            P->getNameAsString(),
            // deduction guides cannot have default arguments
            std::string());
    }

    populate(I.Explicit, D->getExplicitSpecifier());

    populateNamespaces(I, D);
}

void
ASTVisitor::
populate(
    NamespaceAliasInfo& I,
    bool const isNew,
    NamespaceAliasDecl* D)
{
    bool const documented = generateJavadoc(I.javadoc, D);
    populate(I, D->getBeginLoc(), true, documented);

    if (!isNew)
    {
        return;
    }

    I.Name = extractName(D);
    auto const& Underlying = I.AliasedSymbol =
        std::make_unique<NameInfo>();

    NamedDecl* Aliased = D->getAliasedNamespace();
    Underlying->Name = Aliased->getIdentifier()->getName();
    upsertDependency(Aliased, Underlying->id);
    if (NestedNameSpecifier const* NNS = D->getQualifier())
    {
        Underlying->Prefix = toNameInfo(NNS);
    }

    populateNamespaces(I, D);
}

void
ASTVisitor::
populate(
    UsingInfo& I,
    bool const isNew,
    UsingDecl* D)
{
    bool const documented = generateJavadoc(I.javadoc, D);
    populate(I, D->getBeginLoc(), true, documented);

    if (!isNew)
    {
        return;
    }

    I.Name = extractName(D);
    I.Class = UsingClass::Normal;
    I.Qualifier = toNameInfo(D->getQualifier());

    for (auto const* UDS: D->shadows())
    {
        upsertDependency(
            UDS->getTargetDecl(),
            I.UsingSymbols.emplace_back());
    }
    populateNamespaces(I, D);
}

void
ASTVisitor::
populate(
    ConceptInfo& I,
    bool const isNew,
    ConceptDecl* D)
{
    bool const documented = generateJavadoc(I.javadoc, D);
    populate(I, D->getBeginLoc(), true, documented);
    if (!isNew)
    {
        return;
    }

    I.Name = extractName(D);
    populate(I.Constraint, D->getConstraintExpr());

    populateNamespaces(I, D);
}

void
ASTVisitor::
populate(
    SourceInfo& I,
    clang::SourceLocation const loc,
    bool const definition,
    bool const documented)
{
    unsigned line = source_.getPresumedLoc(
        loc, false).getLine();
    FileInfo* file = findFileInfo(loc);
    MRDOCS_ASSERT(file);

    if (definition)
    {
        if (I.DefLoc)
        {
            return;
        }
        I.DefLoc.emplace(file->full_path,
            file->short_path, line, file->kind,
            documented);
    }
    else
    {
        auto const existing = std::ranges::
            find_if(I.Loc,
            [line, file](const Location& l)
            {
                return l.LineNumber == line &&
                    l.Path == file->full_path;
            });
        if (existing != I.Loc.end())
        {
            return;
        }
        I.Loc.emplace_back(file->full_path,
            file->short_path, line, file->kind,
            documented);
    }
}

/*  Default function to populate template parameters
 */
template <
    std::derived_from<Decl> DeclTy,
    std::derived_from<TemplateDecl> TemplateDeclTy>
void
ASTVisitor::
populate(TemplateInfo& Template, DeclTy*, TemplateDeclTy* TD)
{
    MRDOCS_ASSERT(TD);
    populate(Template, TD->getTemplateParameters());
}

template<std::derived_from<CXXRecordDecl> CXXRecordDeclTy>
void
ASTVisitor::
populate(
    TemplateInfo& Template,
    CXXRecordDeclTy* D,
    ClassTemplateDecl* CTD)
{
    MRDOCS_ASSERT(CTD);

    // If D is a partial/explicit specialization, extract the template arguments
    if (auto* CTSD = dyn_cast<ClassTemplateSpecializationDecl>(D))
    {
        generateID(getInstantiatedFrom(CTD), Template.Primary);

        // Extract the template arguments of the specialization
        populate(Template.Args, CTSD->getTemplateArgsAsWritten());

        // Extract the template parameters if this is a partial specialization
        if (auto* CTPSD = dyn_cast<ClassTemplatePartialSpecializationDecl>(D))
        {
            populate(Template, CTPSD->getTemplateParameters());
        }
    }
    else
    {
        // Otherwise, extract the template parameter list from CTD
        populate(Template, CTD->getTemplateParameters());
    }
}

template<std::derived_from<VarDecl> VarDeclTy>
void
ASTVisitor::
populate(
    TemplateInfo& Template,
    VarDeclTy* D,
    VarTemplateDecl* VTD)
{
    MRDOCS_ASSERT(VTD);

    // If D is a partial/explicit specialization, extract the template arguments
    if(auto* VTSD = dyn_cast<VarTemplateSpecializationDecl>(D))
    {
        generateID(getInstantiatedFrom(VTD), Template.Primary);
        // extract the template arguments of the specialization
        populate(Template.Args, VTSD->getTemplateArgsAsWritten());
        // extract the template parameters if this is a partial specialization
        if(auto* VTPSD = dyn_cast<VarTemplatePartialSpecializationDecl>(D))
            populate(Template, VTPSD->getTemplateParameters());
    }
    else
    {
        // otherwise, extract the template parameter list from VTD
        populate(Template, VTD->getTemplateParameters());
    }
}


void
ASTVisitor::
populate(
    NoexceptInfo& I,
    const FunctionProtoType* FPT)
{
    MRDOCS_ASSERT(FPT);
    I.Implicit = ! FPT->hasNoexceptExceptionSpec();
    I.Kind = convertToNoexceptKind(
        FPT->getExceptionSpecType());
    // store the operand, if any
    if (Expr const* NoexceptExpr = FPT->getNoexceptExpr())
    {
        I.Operand = toString(NoexceptExpr);
    }
}

void
ASTVisitor::
populate(
    ExplicitInfo& I,
    const ExplicitSpecifier& ES)
{
    I.Implicit = ! ES.isSpecified();
    I.Kind = convertToExplicitKind(ES);

    // store the operand, if any
    if (Expr const* ExplicitExpr = ES.getExpr())
    {
        I.Operand = toString(ExplicitExpr);
    }
}

void
ASTVisitor::
populate(
    ExprInfo& I,
    const Expr* E)
{
    if (!E)
    {
        return;
    }
    I.Written = getSourceCode(
        E->getSourceRange());
}

template <class T>
void
ASTVisitor::
populate(
    ConstantExprInfo<T>& I,
    const Expr* E)
{
    populate(static_cast<ExprInfo&>(I), E);
    // if the expression is dependent,
    // we cannot get its value
    if (!E || E->isValueDependent())
    {
        return;
    }
    I.Value.emplace(toInteger<T>(E->EvaluateKnownConstInt(context_)));
}

template <class T>
void
ASTVisitor::
populate(
    ConstantExprInfo<T>& I,
    const Expr* E,
    const llvm::APInt& V)
{
    populate(I, E);
    I.Value.emplace(toInteger<T>(V));
}

template
void
ASTVisitor::
populate<std::uint64_t>(
    ConstantExprInfo<std::uint64_t>& I,
    const Expr* E,
    const llvm::APInt& V);

void
ASTVisitor::
populate(
    std::unique_ptr<TParam>& I,
    const NamedDecl* N)
{
    visit(N, [&]<typename DeclTy>(const DeclTy* P)
    {
        constexpr Decl::Kind kind =
            DeclToKind<DeclTy>();

        if constexpr(kind == Decl::TemplateTypeParm)
        {
            if (!I)
            {
                I = std::make_unique<TypeTParam>();
            }
            auto* R = dynamic_cast<TypeTParam*>(I.get());
            if (P->wasDeclaredWithTypename())
            {
                R->KeyKind = TParamKeyKind::Typename;
            }
            if (P->hasDefaultArgument() && !R->Default)
            {
                R->Default = toTArg(
                    P->getDefaultArgument().getArgument());
            }
            if(const TypeConstraint* TC = P->getTypeConstraint())
            {
                const NestedNameSpecifier* NNS =
                    TC->getNestedNameSpecifierLoc().getNestedNameSpecifier();
                std::optional<const ASTTemplateArgumentListInfo*> TArgs;
                if (TC->hasExplicitTemplateArgs())
                {
                    TArgs.emplace(TC->getTemplateArgsAsWritten());
                }
                R->Constraint = toNameInfo(TC->getNamedConcept(), TArgs, NNS);
            }
            return;
        }
        else if constexpr(kind == Decl::NonTypeTemplateParm)
        {
            if (!I)
            {
                I = std::make_unique<NonTypeTParam>();
            }
            auto* R = dynamic_cast<NonTypeTParam*>(I.get());
            R->Type = toTypeInfo(P->getType());
            if (P->hasDefaultArgument() && !R->Default)
            {
                R->Default = toTArg(
                    P->getDefaultArgument().getArgument());
            }
            return;
        }
        else if constexpr(kind == Decl::TemplateTemplateParm)
        {
            if (!I)
            {
                I = std::make_unique<TemplateTParam>();
            }
            auto* R = dynamic_cast<TemplateTParam*>(I.get());
            if(R->Params.empty())
            {
                for (NamedDecl const* NP: *P->getTemplateParameters())
                {
                    populate(R->Params.emplace_back(), NP);
                }
            }
            if (P->hasDefaultArgument() && !R->Default)
            {
                R->Default = toTArg(
                    P->getDefaultArgument().getArgument());
            }
            return;
        }
        MRDOCS_UNREACHABLE();
    });

    if (I->Name.empty())
    {
        I->Name = extractName(N);
    }
    // KRYSTIAN NOTE: Decl::isParameterPack
    // returns true for function parameter packs
    I->IsParameterPack =
        N->isTemplateParameterPack();
}

void
ASTVisitor::
populate(
    TemplateInfo& TI,
    TemplateParameterList const* TPL)
{
    if (TPL->size() > TI.Params.size())
    {
        TI.Params.resize(TPL->size());
    }
    for (std::size_t I = 0; I < TPL->size(); ++I)
    {
        populate(TI.Params[I], TPL->getParam(I));
    }
    if (auto* RC = TPL->getRequiresClause())
    {
        populate(TI.Requires, RC);
    }
}

void
ASTVisitor::
populate(
    std::vector<std::unique_ptr<TArg>>& result,
    const ASTTemplateArgumentListInfo* args)
{
    return populate(result,
        std::views::transform(args->arguments(),
            [](auto& x) -> auto&
        {
            return x.getArgument();
        }));
}

std::string
ASTVisitor::
extractName(NamedDecl const* D)
{
    return extractName(D->getDeclName());
}

std::string
ASTVisitor::
extractName(DeclarationName const N)
{
    std::string result;
    if (N.isEmpty())
    {
        return result;
    }
    switch(N.getNameKind())
    {
    case DeclarationName::Identifier:
        if (auto const* I = N.getAsIdentifierInfo())
        {
            result.append(I->getName());
        }
        break;
    case DeclarationName::CXXDestructorName:
        result.push_back('~');
        [[fallthrough]];
    case DeclarationName::CXXConstructorName:
        if (auto const* R = N.getCXXNameType()->getAsCXXRecordDecl())
        {
            result.append(R->getIdentifier()->getName());
        }
        break;
    case DeclarationName::CXXDeductionGuideName:
        if (auto const* T = N.getCXXDeductionGuideTemplate())
        {
            result.append(T->getIdentifier()->getName());
        }
        break;
    case DeclarationName::CXXConversionFunctionName:
    {
        result.append("operator ");
        // KRYSTIAN FIXME: we *really* should not be
        // converting types to strings like this
        result.append(mrdocs::toString(*toTypeInfo(N.getCXXNameType())));
        break;
    }
    case DeclarationName::CXXOperatorName:
    {
        OperatorKind const K = convertToOperatorKind(
            N.getCXXOverloadedOperator());
        result.append("operator");
        std::string_view const name = getOperatorName(K);
        if (std::isalpha(name.front()))
        {
            result.push_back(' ');
        }
        result.append(name);
        break;
    }
    case DeclarationName::CXXLiteralOperatorName:
    case DeclarationName::CXXUsingDirective:
        break;
    default:
        MRDOCS_UNREACHABLE();
    }
    return result;
}

void
ASTVisitor::
populateNamespaces(
    Info& I,
    Decl* D)
{
    Decl* PD = getParentDecl(D);
    SymbolID ParentID = generateID(PD);
    switch(PD->getKind())
    {
    // The TranslationUnit DeclContext is the global namespace;
    // it uses SymbolID::global and should *always* exist
    case Decl::TranslationUnit:
    {
        MRDOCS_ASSERT(ParentID == SymbolID::global);
        auto [P, isNew] = upsert<
            NamespaceInfo>(ParentID);
        addMember(P, I);
        break;
    }
    case Decl::Namespace:
    {
        auto [P, isNew] = upsert<
            NamespaceInfo>(ParentID);
        populate(P, isNew, cast<NamespaceDecl>(PD));
        addMember(P, I);
        break;
    }
    // special case for an explicit specializations of
    // a member of an implicit instantiation.
    case Decl::ClassTemplateSpecialization:
    case Decl::ClassTemplatePartialSpecialization:
    if(auto* S = dyn_cast<ClassTemplateSpecializationDecl>(PD);
        S && S->getSpecializationKind() == TSK_ImplicitInstantiation)
    {
        // KRYSTIAN FIXME: i'm pretty sure DeclContext::getDeclKind()
        // will never be Decl::ClassTemplatePartialSpecialization for
        // implicit instantiations; instead, the ClassTemplatePartialSpecializationDecl
        // is accessible through S->getSpecializedTemplateOrPartial
        // if the implicit instantiation used a partially specialized template,
        MRDOCS_ASSERT(PD->getKind() !=
            Decl::ClassTemplatePartialSpecialization);

        auto [P, isNew] = upsert<
            SpecializationInfo>(ParentID);
        populate(P, isNew, S);
        addMember(P, I);
        break;
    }
    // non-implicit instantiations should be
    // treated like normal CXXRecordDecls
    [[fallthrough]];
    // we should never encounter a Record
    // that is not a CXXRecord
    case Decl::CXXRecord:
    {
        auto [P, isNew] = upsert<
            RecordInfo>(ParentID);
        populate(P, isNew, cast<CXXRecordDecl>(PD));
        addMember(P, I);
        break;
    }
    case Decl::Enum:
    {
        auto [P, isNew] = upsert<
            EnumInfo>(ParentID);
        populate(P, isNew, cast<EnumDecl>(PD));
        addMember(P, I);
        break;
    }
    default:
        MRDOCS_UNREACHABLE();
    }

    Info* P = find(ParentID);
    MRDOCS_ASSERT(P);

    I.Namespace.emplace_back(ParentID);
    I.Namespace.insert(
        I.Namespace.end(),
        P->Namespace.begin(),
        P->Namespace.end());
}

void
ASTVisitor::
addMember(
    ScopeInfo& P,
    Info& C)
{
    // Include C.id in P.Members if it's not already there
    if (bool const exists = std::ranges::find(P.Members, C.id)
                            != P.Members.end();
        !exists)
    {
        P.Members.emplace_back(C.id);
    }

    // Include C.id in P.Lookups[C.Name] if it's not already there
    auto& lookups = P.Lookups.try_emplace(C.Name).first->second;
    if (bool const exists = std::ranges::find(lookups, C.id) != lookups.end();
        !exists)
    {
        lookups.emplace_back(C.id);
    }
}

bool
ASTVisitor::
generateJavadoc(
    std::unique_ptr<Javadoc>& javadoc,
    Decl const* D)
{
    RawComment const* RC =
        D->getASTContext().getRawCommentForDeclNoCache(D);
    if (!RC)
    {
        return false;
    }
    comments::FullComment* FC =
        RC->parse(D->getASTContext(), &sema_.getPreprocessor(), D);
    if (!FC)
    {
        return false;
    }
    // KRYSTIAN FIXME: clang ignores documentation comments
    // when there is a preprocessor directive between the end
    // of the comment and the declaration location. there are two
    // ways to fix this: either set the declaration begin location
    // to be before and preprocessor directives, or submit a patch
    // which disables this behavior (it's not entirely clear why
    // this check occurs anyways, so some investigation is needed)
    parseJavadoc(javadoc, FC, D, config_, diags_);
    return true;
}

std::unique_ptr<TypeInfo>
ASTVisitor::
toTypeInfo(
    QualType const qt,
    ExtractMode const extract_mode)
{
    // extract_mode is only used during the extraction
    // the terminal type & its parents; the extraction of
    // function parameters, template arguments, and the parent class
    // of member pointers is done in ExtractMode::IndirectDependency
    ExtractionScope scope = enterMode(extract_mode);
    // build the TypeInfo representation for the type
    TypeInfoBuilder Builder(*this);
    Builder.Visit(qt);
    return Builder.result();
}

std::unique_ptr<NameInfo>
ASTVisitor::
toNameInfo(
    NestedNameSpecifier const* NNS,
    ExtractMode const extract_mode)
{
    ExtractionScope scope = enterMode(extract_mode);

    std::unique_ptr<NameInfo> I = nullptr;
    if (!NNS)
    {
        return I;
    }

    if (checkSpecialNamespace(I, NNS, nullptr))
    {
        return I;
    }

    if(const Type* T = NNS->getAsType())
    {
        NameInfoBuilder Builder(*this, NNS->getPrefix());
        Builder.Visit(T);
        I = Builder.result();
    }
    else if(const IdentifierInfo* II = NNS->getAsIdentifier())
    {
        I = std::make_unique<NameInfo>();
        I->Name = II->getName();
        I->Prefix = toNameInfo(NNS->getPrefix(), extract_mode);
    }
    else if(const NamespaceDecl* ND = NNS->getAsNamespace())
    {
        I = std::make_unique<NameInfo>();
        I->Name = ND->getIdentifier()->getName();
        upsertDependency(ND, I->id);
        I->Prefix = toNameInfo(NNS->getPrefix(), extract_mode);
    }
    else if(const NamespaceAliasDecl* NAD = NNS->getAsNamespaceAlias())
    {
        I = std::make_unique<NameInfo>();
        I->Name = NAD->getIdentifier()->getName();
        upsertDependency(NAD, I->id);
        I->Prefix = toNameInfo(NNS->getPrefix(), extract_mode);
    }
    return I;
}

template <class TArgRange>
std::unique_ptr<NameInfo>
ASTVisitor::
toNameInfo(
    DeclarationName const Name,
    std::optional<TArgRange> TArgs,
    NestedNameSpecifier const* NNS,
    ExtractMode const extract_mode)
{
    if (Name.isEmpty())
    {
        return nullptr;
    }
    std::unique_ptr<NameInfo> I = nullptr;
    if(TArgs)
    {
        auto Specialization = std::make_unique<SpecializationNameInfo>();
        populate(Specialization->TemplateArgs, *TArgs);
        I = std::move(Specialization);
    }
    else
    {
        I = std::make_unique<NameInfo>();
    }
    I->Name = extractName(Name);
    if (NNS)
    {
        I->Prefix = toNameInfo(NNS, extract_mode);
    }
    return I;
}

template <class TArgRange>
std::unique_ptr<NameInfo>
ASTVisitor::
toNameInfo(
    Decl const* D,
    std::optional<TArgRange> TArgs,
    NestedNameSpecifier const* NNS,
    ExtractMode extract_mode)
{
    const auto* ND = dyn_cast_if_present<NamedDecl>(D);
    if (!ND)
    {
        return nullptr;
    }
    auto I = toNameInfo(ND->getDeclName(),
        std::move(TArgs), NNS, extract_mode);
    if (!I)
    {
        return nullptr;
    }
    upsertDependency(getInstantiatedFrom(D), I->id);
    return I;
}

template
std::unique_ptr<NameInfo>
ASTVisitor::
toNameInfo<llvm::ArrayRef<clang::TemplateArgument>>(
    Decl const* D,
    std::optional<llvm::ArrayRef<clang::TemplateArgument>> TArgs,
    NestedNameSpecifier const* NNS,
    ExtractMode extract_mode);

std::unique_ptr<TArg>
ASTVisitor::
toTArg(const TemplateArgument& A)
{
    // TypePrinter generates an internal placeholder name (e.g. type-parameter-0-0)
    // for template type parameters used as arguments. it also cannonicalizes
    // types, which we do not want (although, PrintingPolicy has an option to change this).
    // thus, we use the template arguments as written.

    // KRYSTIAN NOTE: this can probably be changed to select
    // the argument as written when it is not dependent and is a type.
    // FIXME: constant folding behavior should be consistent with that of other
    // constructs, e.g. noexcept specifiers & explicit specifiers
    switch(A.getKind())
    {
    // empty template argument (e.g. not yet deduced)
    case TemplateArgument::Null:
        break;

    // a template argument pack (any kind)
    case TemplateArgument::Pack:
    {
        // we should never a TemplateArgument::Pack here
        MRDOCS_UNREACHABLE();
        break;
    }
    // type
    case TemplateArgument::Type:
    {
        auto R = std::make_unique<TypeTArg>();
        QualType QT = A.getAsType();
        MRDOCS_ASSERT(! QT.isNull());
        // if the template argument is a pack expansion,
        // use the expansion pattern as the type & mark
        // the template argument as a pack expansion
        if(const Type* T = QT.getTypePtr();
            auto* PT = dyn_cast<PackExpansionType>(T))
        {
            R->IsPackExpansion = true;
            QT = PT->getPattern();
        }
        R->Type = toTypeInfo(QT);
        return R;
    }
    // pack expansion of a template name
    case TemplateArgument::TemplateExpansion:
    // template name
    case TemplateArgument::Template:
    {
        auto R = std::make_unique<TemplateTArg>();
        R->IsPackExpansion = A.isPackExpansion();

        // KRYSTIAN FIXME: template template arguments are
        // id-expression, so we don't properly support them yet.
        // for the time being, we will use the name & SymbolID of
        // the referenced declaration (if it isn't dependent),
        // and fallback to printing the template name otherwise
        TemplateName const TN = A.getAsTemplateOrTemplatePattern();
        if(auto* TD = TN.getAsTemplateDecl())
        {
            if(auto* II = TD->getIdentifier())
                R->Name = II->getName();
            // do not extract a SymbolID or build Info if
            // the template template parameter names a
            // template template parameter or builtin template
            if(! isa<TemplateTemplateParmDecl>(TD) &&
                ! isa<BuiltinTemplateDecl>(TD))
            {
                upsertDependency(getInstantiatedFrom<
                    NamedDecl>(TD), R->Template);
            }
        }
        else
        {
            llvm::raw_string_ostream stream(R->Name);
            TN.print(stream, context_.getPrintingPolicy(),
                TemplateName::Qualified::AsWritten);
        }
        return R;
    }
    // nullptr value
    case TemplateArgument::NullPtr:
    // expression referencing a declaration
    case TemplateArgument::Declaration:
    // integral expression
    case TemplateArgument::Integral:
    // expression
    case TemplateArgument::Expression:
    {
        auto R = std::make_unique<NonTypeTArg>();
        R->IsPackExpansion = A.isPackExpansion();
        // if this is a pack expansion, use the template argument
        // expansion pattern in place of the template argument pack
        const TemplateArgument& adjusted =
            R->IsPackExpansion ?
            A.getPackExpansionPattern() : A;

        llvm::raw_string_ostream stream(R->Value.Written);
        adjusted.print(context_.getPrintingPolicy(), stream, false);

        return R;
    }
    default:
        MRDOCS_UNREACHABLE();
    }
    return nullptr;
}


std::string
ASTVisitor::
toString(const Expr* E)
{
    std::string result;
    llvm::raw_string_ostream stream(result);
    E->printPretty(stream, nullptr, context_.getPrintingPolicy());
    return result;
}

std::string
ASTVisitor::
toString(const Type* T)
{
    if(auto* AT = dyn_cast_if_present<AutoType>(T))
    {
        switch(AT->getKeyword())
        {
        case AutoTypeKeyword::Auto:
        case AutoTypeKeyword::GNUAutoType:
            return "auto";
        case AutoTypeKeyword::DecltypeAuto:
            return "decltype(auto)";
        default:
            MRDOCS_UNREACHABLE();
        }
    }
    if(auto* TTPT = dyn_cast_if_present<TemplateTypeParmType>(T))
    {
        if (TemplateTypeParmDecl* TTPD = TTPT->getDecl();
            TTPD && TTPD->isImplicit())
        {
            return "auto";
        }
    }
    return QualType(T, 0).getAsString(
        context_.getPrintingPolicy());
}

template<class Integer>
Integer
ASTVisitor::
toInteger(const llvm::APInt& V)
{
    if constexpr (std::is_signed_v<Integer>)
    {
        return static_cast<Integer>(V.getSExtValue());
    }
    else
    {
        return static_cast<Integer>(V.getZExtValue());
    }
}

std::string
ASTVisitor::
getSourceCode(SourceRange const& R) const
{
    return Lexer::getSourceText(
        CharSourceRange::getTokenRange(R),
        source_,
        context_.getLangOpts()).str();
}

std::optional<std::pair<QualType, std::vector<TemplateArgument>>>
ASTVisitor::
isSFINAEType(QualType const T)
{
    if (!config_->detectSfinae)
    {
        return std::nullopt;
    }

    auto sfinae_info = getSFINAETemplate(T, true);
    if (!sfinae_info)
    {
        return std::nullopt;
    }

    auto sfinae_result = isSFINAETemplate(
        sfinae_info->Template, sfinae_info->Member);

    if (!sfinae_result)
    {
        return std::nullopt;
    }

    auto [template_params, controlling_params, param_idx] = *sfinae_result;

    auto const Args = sfinae_info->Arguments;
    auto const param_arg = tryGetTemplateArgument(
        template_params, Args, param_idx);
    if (!param_arg)
    {
        return std::nullopt;
    }

    std::vector<TemplateArgument> ControllingArgs;
    for (std::size_t I = 0; I < Args.size(); ++I)
    {
        if (controlling_params[I])
        {
            ControllingArgs.emplace_back(Args[I]);
        }
    }

    return std::make_pair(param_arg->getAsType(), std::move(ControllingArgs));
}

std::optional<std::tuple<TemplateParameterList*, llvm::SmallBitVector, unsigned>>
ASTVisitor::
isSFINAETemplate(
    TemplateDecl* TD,
    const IdentifierInfo* Member)
{
    if (!TD)
    {
        return std::nullopt;
    }

    auto FindParam = [this](
        ArrayRef<TemplateArgument> Arguments,
        const TemplateArgument& Arg) -> unsigned
    {
        if (Arg.getKind() != TemplateArgument::Type)
        {
            return -1;
        }
        auto Found = std::ranges::find_if(Arguments, [&](const TemplateArgument& Other)
        {
            if (Other.getKind() != TemplateArgument::Type)
            {
                return false;
            }
            return context_.hasSameType(Other.getAsType(), Arg.getAsType());
        });
        return Found != Arguments.end() ? Found - Arguments.data() : -1;
    };

    if(auto* ATD = dyn_cast<TypeAliasTemplateDecl>(TD))
    {
        auto Underlying = ATD->getTemplatedDecl()->getUnderlyingType();
        auto sfinae_info = getSFINAETemplate(Underlying, !Member);
        if (!sfinae_info)
        {
            return std::nullopt;
        }
        if (Member)
        {
            sfinae_info->Member = Member;
        }
        auto sfinae_result = isSFINAETemplate(
            sfinae_info->Template, sfinae_info->Member);
        if (!sfinae_result)
        {
            return std::nullopt;
        }
        auto [template_params, controlling_params, param_idx] = *sfinae_result;
        auto param_arg = tryGetTemplateArgument(
            template_params, sfinae_info->Arguments, param_idx);
        if (!param_arg)
        {
            return std::nullopt;
        }
        unsigned ParamIdx = FindParam(ATD->getInjectedTemplateArgs(context_), *param_arg);
        return std::make_tuple(ATD->getTemplateParameters(), std::move(controlling_params), ParamIdx);
    }

    auto* CTD = dyn_cast<ClassTemplateDecl>(TD);
    if (!CTD)
    {
        return std::nullopt;
    }

    auto PrimaryArgs = CTD->getInjectedTemplateArgs(context_);
    llvm::SmallBitVector ControllingParams(PrimaryArgs.size());

    QualType MemberType;
    unsigned ParamIdx = -1;
    auto IsMismatch = [&](CXXRecordDecl* RD, ArrayRef<TemplateArgument> Args)
    {
        if (!RD->hasDefinition())
        {
            return false;
        }
        auto MemberLookup = RD->lookup(Member);
        QualType CurrentType;
        if(MemberLookup.empty())
        {
            if (!RD->getNumBases())
            {
                return false;
            }
            for(auto& Base : RD->bases())
            {
                auto sfinae_info = getSFINAETemplate(Base.getType(), false);
                if(! sfinae_info)
                {
                    // if the base is an opaque dependent type, we can't determine
                    // whether it's a SFINAE type
                    if (Base.getType()->isDependentType())
                    {
                        return true;
                    }
                    continue;
                }
                // if the class inherits from itself, we can't determine whether
                // it's a SFINAE type
                if (declaresSameEntity(TD, sfinae_info->Template))
                {
                    return true;
                }

                auto sfinae_result = isSFINAETemplate(
                    sfinae_info->Template, Member);
                if (!sfinae_result)
                {
                    return true;
                }

                auto [template_params, controlling_params, param_idx] = *sfinae_result;
                auto param_arg = tryGetTemplateArgument(
                    template_params, sfinae_info->Arguments, param_idx);
                if(! param_arg)
                    return true;
                auto CurrentTypeFromBase = param_arg->getAsType();
                if (CurrentType.isNull())
                {
                    CurrentType = CurrentTypeFromBase;
                }
                else if (
                    !context_.hasSameType(CurrentType, CurrentTypeFromBase))
                {
                    return true;
                }
            }
            // didn't find a base that defines the specified member
            if (CurrentType.isNull())
            {
                return false;
            }
        }
        else
        {
            // ambiguous lookup
            if (!MemberLookup.isSingleResult())
            {
                return true;
            }
            if (auto* TND = dyn_cast<TypedefNameDecl>(MemberLookup.front()))
            {
                CurrentType = TND->getUnderlyingType();
            } else
            {
                // the specialization has a member with the right name,
                // but it isn't an alias declaration/typedef declaration...
                return true;
            }
        }

        if(CurrentType->isDependentType())
        {
            auto FoundIdx = FindParam(Args, TemplateArgument(CurrentType));
            if (FoundIdx == static_cast<decltype(FoundIdx)>(-1)
                || FoundIdx >= PrimaryArgs.size())
            {
                return true;
            }
            ParamIdx = FoundIdx;
            TemplateArgument MappedPrimary = PrimaryArgs[FoundIdx];
            assert(MappedPrimary.getKind() == TemplateArgument::Type);
            CurrentType = MappedPrimary.getAsType();
        }

        if (MemberType.isNull())
        {
            MemberType = CurrentType;
        }

        return ! context_.hasSameType(MemberType, CurrentType);
    };

    if (IsMismatch(CTD->getTemplatedDecl(), PrimaryArgs))
    {
        return std::nullopt;
    }

    for(auto* CTSD : CTD->specializations())
    {
        if (CTSD->isExplicitSpecialization()
            && IsMismatch(CTSD, CTSD->getTemplateArgs().asArray()))
        {
            return std::nullopt;
        }
    }

    SmallVector<ClassTemplatePartialSpecializationDecl*> PartialSpecs;
    CTD->getPartialSpecializations(PartialSpecs);

    for(auto* CTPSD : PartialSpecs)
    {
        auto PartialArgs = CTPSD->getTemplateArgs().asArray();
        if (IsMismatch(CTPSD, PartialArgs))
        {
            return std::nullopt;
        }
        for(std::size_t I = 0; I < PartialArgs.size(); ++I)
        {
            TemplateArgument Arg = PartialArgs[I];
            switch(Arg.getKind())
            {
            case TemplateArgument::Integral:
            case TemplateArgument::Declaration:
            case TemplateArgument::StructuralValue:
            case TemplateArgument::NullPtr:
                break;
            case TemplateArgument::Expression:
                if(getNTTPFromExpr(
                    Arg.getAsExpr(),
                    CTPSD->getTemplateDepth() - 1))
                    continue;
                break;
            default:
                continue;
            }
            ControllingParams.set(I);
            //.getAsExpr()
        }
    }

    return std::make_tuple(CTD->getTemplateParameters(), std::move(ControllingParams), ParamIdx);
}

std::optional<ASTVisitor::SFINAEInfo>
ASTVisitor::
getSFINAETemplate(QualType T, bool const AllowDependentNames)
{
    assert(!T.isNull());
    SFINAEInfo SFINAE;
    if (auto* ET = T->getAs<ElaboratedType>())
    {
        T = ET->getNamedType();
    }

    if(auto* DNT = T->getAsAdjusted<DependentNameType>();
        DNT && AllowDependentNames)
    {
        SFINAE.Member = DNT->getIdentifier();
        T = QualType(DNT->getQualifier()->getAsType(), 0);
    }

    if(auto* TST = T->getAsAdjusted<TemplateSpecializationType>())
    {
        SFINAE.Template = TST->getTemplateName().getAsTemplateDecl();
        SFINAE.Arguments = TST->template_arguments();
        return SFINAE;
    }
    return std::nullopt;
}

std::optional<TemplateArgument>
ASTVisitor::
tryGetTemplateArgument(
    TemplateParameterList* Parameters,
    ArrayRef<TemplateArgument> Arguments,
    unsigned const Index)
{
    if (Index == static_cast<unsigned>(-1))
    {
        return std::nullopt;
    }
    if (Index < Arguments.size())
    {
        return Arguments[Index];
    }
    if(Parameters && Index < Parameters->size())
    {
        NamedDecl* ND = Parameters->getParam(Index);
        if(auto* TTPD = dyn_cast<TemplateTypeParmDecl>(ND);
            TTPD && TTPD->hasDefaultArgument())
        {
            return TTPD->getDefaultArgument().getArgument();
        }
        if(auto* NTTPD = dyn_cast<NonTypeTemplateParmDecl>(ND);
            NTTPD && NTTPD->hasDefaultArgument())
        {
            return NTTPD->getDefaultArgument().getArgument();
        }
    }
    return std::nullopt;
}

bool
ASTVisitor::
shouldExtract(
    const Decl* D,
    AccessSpecifier const access)
{
    using enum ConfigImpl::SettingsImpl::ExtractPolicy;
    if (config_->inaccessibleMembers != Always)
    {
        // KRYSTIAN FIXME: this doesn't handle direct
        // dependencies on inaccessible declarations
        MRDOCS_CHECK_OR(
             access != AccessSpecifier::AS_private &&
             access != AccessSpecifier::AS_protected, false);
    }

    // Don't extract anonymous unions
    const auto* RD = dyn_cast<RecordDecl>(D);
    MRDOCS_CHECK_OR(!RD || !RD->isAnonymousStructOrUnion(), false);

    // Don't extract implicitly generated declarations
    // (except for IndirectFieldDecls)
    MRDOCS_CHECK_OR(!D->isImplicit() || isa<IndirectFieldDecl>(D), false);

    // Don't extract declarations that fail the input filter
    if (!config_->input.include.empty())
    {
        // Get filename
        FileInfo* file = findFileInfo(D->getBeginLoc());
        MRDOCS_CHECK_OR(file, false);
        std::string filename = file->full_path;
        MRDOCS_CHECK_OR(std::ranges::any_of(
            config_->input.include,
            [&filename](const std::string& prefix)
            {
                return files::startsWith(filename, prefix);
            }), false);
    }

    // Don't extract declarations that fail the file pattern filter
    if (!config_->input.filePatterns.empty())
    {
        // Get filename
        FileInfo* file = findFileInfo(D->getBeginLoc());
        MRDOCS_CHECK_OR(file, false);
        std::string filename = file->full_path;
        MRDOCS_CHECK_OR(std::ranges::any_of(
            config_->input.filePatterns,
            [&filename](const std::string& pattern)
            {
                return globMatch(pattern, filename);
            }), false);
    }

    // Don't extract declarations that fail the symbol filter
    MRDOCS_CHECK_OR(
        currentMode() != ExtractMode::Normal ||
        inExtractedFile(D), false);

    // Don't extract anonymous namespaces unless configured to do so
    if(const auto* ND = dyn_cast<NamespaceDecl>(D);
        ND &&
        ND->isAnonymousNamespace() &&
        config_->anonymousNamespaces != Always)
    {
        // Always skip anonymous namespaces if so configured
        MRDOCS_CHECK_OR(config_->anonymousNamespaces != Never, false);

        // Otherwise, skip extraction if this isn't a dependency
        // KRYSTIAN FIXME: is this correct? a namespace should not
        // be extracted as a dependency (until namespace aliases and
        // using directives are supported)
        MRDOCS_CHECK_OR(currentMode() != ExtractMode::Normal, false);
    }

    return true;
}

bool
ASTVisitor::
checkSymbolFilter(NamedDecl const* ND)
{
    if (currentMode() != ExtractMode::Normal ||
        symbolFilter_.detached)
    {
        return true;
    }

    std::string const name = extractName(ND);
    const FilterNode* parent = symbolFilter_.current;
    if(const FilterNode* child = parent->findChild(name))
    {
        // if there is a matching node, skip extraction if it's
        // explicitly excluded AND has no children. the presence
        // of child nodes indicates that some child exists that
        // is explicitly whitelisted
        if (child->Explicit &&
            child->Excluded &&
            child->isTerminal())
        {
            return false;
        }
        symbolFilter_.setCurrent(child, false);
    }
    else
    {
        // if there was no matching node, check the most
        // recently entered explicitly specified parent node.
        // if it's blacklisted, then the "filtering default"
        // is to exclude symbols unless a child is explicitly
        // whitelisted
        if (symbolFilter_.last_explicit
            && symbolFilter_.last_explicit->Excluded)
        {
            return false;
        }

        if (const auto* DC = dyn_cast<DeclContext>(ND);
            !DC ||
            !DC->isInlineNamespace())
        {
            // if this namespace does not match a child
            // of the current filter node, set the detached flag
            // so we don't update the namespace filter state
            // while traversing the children of this namespace
            symbolFilter_.detached = true;
        }
    }
    return true;
}

// This also sets IsFileInRootDir
bool
ASTVisitor::
inExtractedFile(
    const Decl* D)
{
    namespace path = llvm::sys::path;

    if(const auto* ND = dyn_cast<NamedDecl>(D))
    {
        // out-of-line declarations require us to rebuild
        // the symbol filtering state
        if(ND->isOutOfLine())
        {
            symbolFilter_.setCurrent(
                &symbolFilter_.root, false);

            // collect all parent classes/enums/namespaces
            llvm::SmallVector<const NamedDecl*, 8> parents;
            const Decl* P = ND;
            while((P = getParentDecl(P)))
            {
                if (isa<TranslationUnitDecl>(P))
                {
                    break;
                }
                parents.push_back(cast<NamedDecl>(P));
            }

            // check whether each parent passes the symbol filters
            // as-if the declaration was inline
            for(const auto* PND : std::views::reverse(parents))
            {
                if (!checkSymbolFilter(PND))
                {
                    return false;
                }
            }
        }

        if (!checkSymbolFilter(ND))
        {
            return false;
        }
    }

    FileInfo const* file = findFileInfo(D->getBeginLoc());
    // KRYSTIAN NOTE: I'm unsure under what conditions
    // this assert would fire.
    MRDOCS_ASSERT(file);
    // only extract from files in source root
    return file->kind == FileKind::Source;
}

bool
ASTVisitor::
isInSpecialNamespace(
    const Decl* D,
    std::span<const FilterPattern> const Patterns)
{
    // Check if a Decl is in a special namespace
    // A Decl is in a special namespace if any of its
    // parent namespaces match a special namespace pattern
    if (!D || Patterns.empty())
    {
        return false;
    }
    const DeclContext* DC = isa<DeclContext>(D) ?
        dyn_cast<DeclContext>(D) : D->getDeclContext();
    for(; DC; DC = DC->getParent())
    {
        const auto* ND = dyn_cast<NamespaceDecl>(DC);
        if (!ND)
        {
            continue;
        }
        for (auto const& Pattern: Patterns)
        {
            if (Pattern.matches(ND->getQualifiedNameAsString()))
            {
                return true;
            }
        }
    }
    return false;
}

bool
ASTVisitor::
isInSpecialNamespace(
    const NestedNameSpecifier* NNS,
    std::span<const FilterPattern> Patterns)
{
    // Check if a NestedNameSpecifier is in a special namespace
    // It's in a special namespace if any of its prefixes are
    // in a special namespace
    const NamedDecl* ND = nullptr;
    while(NNS)
    {
        if ((ND = NNS->getAsNamespace()))
        {
            break;
        }
        if ((ND = NNS->getAsNamespaceAlias()))
        {
            break;
        }
        NNS = NNS->getPrefix();
    }
    return ND && isInSpecialNamespace(ND, Patterns);
}

bool
ASTVisitor::
checkSpecialNamespace(
    std::unique_ptr<NameInfo>& I,
    const NestedNameSpecifier* NNS,
    const Decl* D) const
{
    // Check if a NestedNameSpecifier or a Decl is in a special namespace
    // If so, update the NameInfo's Name to reflect this
    if (isInSpecialNamespace(NNS, config_->seeBelowFilter) ||
        isInSpecialNamespace(D, config_->seeBelowFilter))
    {
        I = std::make_unique<NameInfo>();
        I->Name = "see-below";
        return true;
    }

    if(isInSpecialNamespace(NNS, config_->implementationDefinedFilter) ||
        isInSpecialNamespace(D, config_->implementationDefinedFilter))
    {
        I = std::make_unique<NameInfo>();
        I->Name = "implementation-defined";
        return true;
    }

    return false;
}

bool
ASTVisitor::
checkSpecialNamespace(
    std::unique_ptr<TypeInfo>& I,
    const NestedNameSpecifier* NNS,
    const Decl* D) const
{
    // Check if a NestedNameSpecifier or a Decl is in a special namespace
    // If so, update the TypeInfo's Name to reflect this
    if (std::unique_ptr<NameInfo> Name;
        checkSpecialNamespace(Name, NNS, D))
    {
        auto T = std::make_unique<NamedTypeInfo>();
        T->Name = std::move(Name);
        I = std::move(T);
        return true;
    }
    return false;
}


Info*
ASTVisitor::
find(SymbolID const& id)
{
    if (auto const it = info_.find(id); it != info_.end())
    {
        return it->get();
    }
    return nullptr;
}

// KRYSTIAN NOTE: we *really* should not have a
// type named "SourceLocation"...
ASTVisitor::FileInfo*
ASTVisitor::
findFileInfo(clang::SourceLocation const loc)
{
    // KRYSTIAN FIXME: we really should not be
    // calling getPresumedLoc this often,
    // it's quite expensive
    auto const presumed = source_.getPresumedLoc(loc, false);
    if (presumed.isInvalid())
    {
        return nullptr;
    }
    const FileEntry* file =
        source_.getFileEntryForID(
            presumed.getFileID());
    // KRYSTIAN NOTE: i have no idea under what
    // circumstances the file entry would be null
    if (!file)
    {
        return nullptr;
    }
    // KRYSTIAN NOTE: i have no idea under what
    // circumstances the file would not be in either
    // the main file, or an included file
    auto const it = files_.find(file);
    if (it == files_.end())
    {
        return nullptr;
    }
    return &it->second;
}

template <std::derived_from<Info> InfoTy>
ASTVisitor::upsertResult<InfoTy>
ASTVisitor::
upsert(SymbolID const& id)
{
    // Creating symbol with invalid SymbolID
    MRDOCS_ASSERT(id != SymbolID::invalid);
    Info* info = find(id);
    bool const isNew = !info;
    if (!info)
    {
        info = info_.emplace(std::make_unique<
            InfoTy>(id)).first->get();
        info->Implicit = currentMode() != ExtractMode::Normal;
    }
    MRDOCS_ASSERT(info->Kind == InfoTy::kind_id);
    return {static_cast<InfoTy&>(*info), isNew};
}

template <std::derived_from<Decl> DeclType>
Expected<ASTVisitor::upsertResult<InfoTypeFor_t<DeclType>>>
ASTVisitor::
upsert(DeclType* D)
{
    AccessSpecifier access = getAccess(D);
    MRDOCS_CHECK_MSG(
        shouldExtract(D, access),
        "Symbol should not be extracted");

    SymbolID const id = generateID(D);
    MRDOCS_CHECK_MSG(id, "Failed to extract symbol ID");

    auto [I, isNew] = upsert<InfoTypeFor_t<DeclType>>(id);
    I.Access = convertToAccessKind(access);

    using R = upsertResult<InfoTypeFor_t<DeclType>>;
    return R{std::ref(I), isNew};
}

void
ASTVisitor::
upsertDependency(Decl* D, SymbolID& id)
{
    if (TemplateDecl* TD = D->getDescribedTemplate())
    {
        D = TD;
    }

    if (D->isImplicit() || isa<TemplateTemplateParmDecl>(D)
        || isa<BuiltinTemplateDecl>(D))
    {
        return;
    }

    id = generateID(D);

    // Don't register a dependency if we never extract them
    if (config_->referencedDeclarations
        == ConfigImpl::SettingsImpl::ExtractPolicy::Never)
    {
        return;
    }

    if(config_->referencedDeclarations ==
        ConfigImpl::SettingsImpl::ExtractPolicy::Dependency)
    {
        if (currentMode() != ExtractMode::DirectDependency)
        {
            return;
        }
    }

    // If it was already extracted, we are done
    if (find(id))
    {
        return;
    }

    // FIXME: we need to handle member specializations correctly.
    // we do not want to extract all members of the enclosing
    // implicit instantiation
    Decl* Outer = D;
    DeclContext* DC = D->getDeclContext();
    do
    {
        if (DC->isFileContext() ||
            DC->isFunctionOrMethod())
        {
            break;
        }

        if (auto const* RD = dyn_cast<CXXRecordDecl>(DC);
            RD &&
            RD->isAnonymousStructOrUnion())
        {
            continue;
        }

        // when extracting dependencies, we want to extract
        // all members of the containing class, not just this one
        if (auto* TD = dyn_cast<TagDecl>(DC))
        {
            Outer = TD;
        }
    }
    while((DC = DC->getParent()));

    if (TemplateDecl* TD = Outer->getDescribedTemplate())
    {
        Outer = TD;
    }

    // Add the adjusted declaration to the set of dependencies
    if (!isa<NamespaceDecl, TranslationUnitDecl>(Outer))
    {
        dependencies_.insert(Outer);
    }
}

} // clang::mrdocs
