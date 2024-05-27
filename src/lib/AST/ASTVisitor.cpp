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

#include "ASTVisitor.hpp"
#include "ASTVisitorHelpers.hpp"
#include "Bitcode.hpp"
#include "ParseJavadoc.hpp"
#include "lib/Support/Path.hpp"
#include "lib/Support/Debug.hpp"
#include "lib/Support/Glob.hpp"
#include "lib/Lib/Diagnostics.hpp"
#include "lib/Lib/Filters.hpp"
#include "lib/Lib/Info.hpp"
#include <mrdocs/Metadata.hpp>
#include <clang/AST/AST.h>
#include <clang/AST/Attr.h>
#include <clang/AST/DeclVisitor.h>
#include <clang/AST/TypeVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Index/USRGeneration.h>
#include <clang/Lex/Lexer.h>
#include <clang/Parse/ParseAST.h>
#include <clang/Sema/Lookup.h>
#include <clang/Sema/Sema.h>
#include <clang/Sema/SemaConsumer.h>
#include <clang/Sema/TemplateInstCallback.h>
#include <llvm/ADT/Hashing.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/SHA1.h>
#include <memory>
#include <optional>
#include <ranges>
#include <unordered_map>
#include <unordered_set>

namespace clang {
namespace mrdocs {

namespace {

struct SymbolFilter
{
    const FilterNode& root;
    const FilterNode* current = nullptr;
    const FilterNode* last_explicit = nullptr;
    bool detached = false;

    SymbolFilter(const FilterNode& root_node)
        : root(root_node)
    {
        setCurrent(&root, false);
    }

    SymbolFilter(const SymbolFilter&) = delete;
    SymbolFilter(SymbolFilter&&) = delete;

    void
    setCurrent(
        const FilterNode* node,
        bool node_detached)
    {
        current = node;
        detached = node_detached;
        if(node && node->Explicit)
            last_explicit = node;
    }

    class FilterScope
    {
        SymbolFilter& filter_;
        const FilterNode* current_prev_;
        const FilterNode* last_explicit_prev_;
        bool detached_prev_;

    public:
        FilterScope(SymbolFilter& filter)
            : filter_(filter)
            , current_prev_(filter.current)
            , last_explicit_prev_(filter.last_explicit)
            , detached_prev_(filter.detached)
        {
        }

        ~FilterScope()
        {
            filter_.current = current_prev_;
            filter_.last_explicit = last_explicit_prev_;
            filter_.detached = detached_prev_;
        }
    };
};

//------------------------------------------------
//
// ASTVisitor
//
//------------------------------------------------

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
{
public:
    const ConfigImpl& config_;
    Diagnostics diags_;

    CompilerInstance& compiler_;
    ASTContext& context_;
    SourceManager& source_;
    Sema& sema_;

    InfoSet info_;
    std::unordered_set<Decl*> dependencies_;

    struct FileInfo
    {
        FileInfo(std::string_view path)
            : full_path(path)
            , short_path(full_path)
        {
        }

        std::string full_path;
        std::string_view short_path;
        FileKind kind;
    };

    std::unordered_map<
        const FileEntry*,
        FileInfo> files_;

    llvm::SmallString<128> usr_;

    SymbolFilter symbolFilter_;

    enum class ExtractMode
    {
        // extraction of declarations which pass all filters
        Normal,
        // extraction of declarations as direct dependencies
        DirectDependency,
        // extraction of declarations as indirect dependencies
        IndirectDependency,
    };

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
    enterMode(
        ExtractMode new_mode) noexcept
    {
        return ExtractionScope(*this, new_mode);
    }

    ExtractMode
    currentMode() const noexcept
    {
        return mode;
    }

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
        // install handlers for our custom commands
        initCustomCommentCommands(context_);

        // the traversal scope should *only* consist of the
        // top-level TranslationUnitDecl. if this assert fires,
        // then it means ASTContext::setTraversalScope is being
        // (erroneously) used somewhere
        MRDOCS_ASSERT(context_.getTraversalScope() ==
            std::vector<Decl*>{context_.getTranslationUnitDecl()});

        using namespace llvm::sys;
        Preprocessor& PP = sema_.getPreprocessor();
        HeaderSearch& HS = PP.getHeaderSearchInfo();

        auto normalize_path = [&](
            std::string_view old_path,
            bool remove_filename)
        {
            llvm::SmallString<128> new_path(old_path);
            if(remove_filename)
                path::remove_filename(new_path);
            // KRYSTIAN FIXME: use FileManager::makeAbsolutePath?
            if(! path::is_absolute(new_path))
            {
                auto& cwd = source_.getFileManager().
                    getFileSystemOpts().WorkingDir;
                // we can't normalize a relative path
                // without a base directory
                // MRDOCS_ASSERT(! cwd.empty());
                fs::make_absolute(cwd, new_path);
            }
            // remove ./ and ../
            path::remove_dots(new_path, true, path::Style::posix);
            // convert to posix style
            path::native(new_path, path::Style::posix);
            return std::string(new_path);
        };

        std::string source_root = normalize_path(
            config_->sourceRoot, true);
        std::vector<std::pair<std::string, FileKind>> search_dirs;

        search_dirs.reserve(HS.search_dir_size());
        // first, convert all the include search directories into POSIX style
        for(const DirectoryLookup& DL : HS.search_dir_range())
        {
            OptionalDirectoryEntryRef DR = DL.getDirRef();
            // only consider normal directories
            if(! DL.isNormalDir() || ! DR)
                continue;
            // store the normalized path
            search_dirs.emplace_back(
                normalize_path(DR->getName(), false),
                DL.isSystemHeaderDirectory() ?
                    FileKind::System : FileKind::Other);
        }

        auto build_file_info = [&](const FileEntry* file)
        {
            // "try" implies this may fail, so fallback to getName
            // if an empty string is returned
            std::string_view file_path =
                file->tryGetRealPathName();
            files_.emplace(file,
                getFileInfo(search_dirs,
                    normalize_path(file_path, false),
                    source_root));
        };

        // build the file info for the main file
        build_file_info(
            source_.getFileEntryForID(
                source_.getMainFileID()));

        // build the file info for all included files
        for(const FileEntry* file : PP.getIncludedFiles())
            build_file_info(file);
    }

    FileInfo
    getFileInfo(
        std::span<const std::pair<
            std::string, FileKind>> search_dirs,
        std::string_view file_path,
        std::string_view source_root)
    {
        using namespace llvm::sys;
        FileInfo file_info(file_path);

        std::ptrdiff_t best_length = 0;
        auto check_dir = [&](
            std::string_view dir_path,
            FileKind kind)
        {
            auto NI = path::begin(file_path), NE = path::end(file_path);
            auto DI = path::begin(dir_path), DE = path::end(dir_path);

            for(; NI != NE; ++NI, ++DI)
            {
                // reached the end of the directory path
                if(DI == DE)
                {
                    // update the best prefix length
                    if(std::ptrdiff_t length =
                        NI - path::begin(file_path);
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
                    path::is_separator(NI->front()) &&
                    path::is_separator(DI->front()))
                    continue;
                // components don't match
                if(*NI != *DI)
                    break;
            }
            return false;
        };

        bool in_source_root = check_dir(
            source_root, FileKind::Source);

        // only use a source_root relative path if we
        // don't find anything in the include search directories
        bool any_match = false;
        for(const auto& [dir_path, kind] : search_dirs)
            any_match |= check_dir(dir_path, kind);

        // KRYSTIAN TODO: if we don't find any matches,
        // make the path relative to source_root and return it
        static_cast<void>(any_match);

        // override the file kind if
        // the file was found in source_root
        if(in_source_root)
            file_info.kind = FileKind::Source;

        file_info.short_path.remove_prefix(best_length);
        return file_info;
    }

    InfoSet& results()
    {
        return info_;
    }

    void build()
    {
        // traverse the translation unit, only extracting
        // declarations which satisfy all filter conditions.
        // dependencies will be tracked, but not extracted
        traverseDecl(context_.getTranslationUnitDecl());

        // if dependency extraction is disabled, we are done
        if(config_->referencedDeclarations ==
            ConfigImpl::SettingsImpl::ExtractPolicy::Never)
            return;

        // traverse the current set of dependencies,
        // and generate a new set based on the results.
        // if the new set is non-empty, perform another pass.
        // do this until no new dependencies are generated
        std::unordered_set<Decl*> previous;
        buildDependencies(previous);
    }

    void buildDependencies(
        std::unordered_set<Decl*>& previous)
    {
        auto scope = enterMode(
            ExtractMode::DirectDependency);

        previous.clear();
        dependencies_.swap(previous);

        for(Decl* D : previous)
        {
            // skip declarations which generate invalid symbol IDs,
            // or which already have been extract
            if(SymbolID id = extractSymbolID(D);
                ! id || info_.contains(id))
                continue;
            traverseDecl(D);
        }

        // perform another pass if there are new dependencies
        if(! dependencies_.empty())
            buildDependencies(previous);
    }

    //------------------------------------------------

    /** Get Info from ASTVisitor InfoSet
     */
    Info*
    getInfo(SymbolID const& id)
    {
        if(auto it = info_.find(id); it != info_.end())
            return it->get();
        return nullptr;
    }

    /** Get or create the Info for a declaration.

        This function will return the Info for a declaration
        if it has already been extracted, or create a new
        Info for the declaration if it has not been extracted.

        @return a pair containing a reference to the Info
        and a boolean indicating whether the Info was created.
    */
    template<typename InfoTy>
    std::pair<InfoTy&, bool>
    getOrCreateInfo(const SymbolID& id)
    {
        Info* info = getInfo(id);
        bool created = ! info;
        if(! info)
        {
            info = info_.emplace(std::make_unique<
                InfoTy>(id)).first->get();
            if(currentMode() != ExtractMode::Normal)
                info->Implicit = true;
        }
        MRDOCS_ASSERT(info->Kind == InfoTy::kind_id);
        return {static_cast<InfoTy&>(*info), created};
    }

    void
    getDependencyID(
        const Decl* D,
        SymbolID& id)
    {
        return getDependencyID(const_cast<Decl*>(D), id);
    }

    void
    getDependencyID(
        Decl* D,
        SymbolID& id)
    {
        if(TemplateDecl* TD = D->getDescribedTemplate())
            D = TD;

        if(D->isImplicit() ||
            isa<TemplateTemplateParmDecl>(D) ||
            isa<BuiltinTemplateDecl>(D))
            return;

        id = extractSymbolID(D);

        // don't register a dependency if we never extract them
        if(config_->referencedDeclarations ==
            ConfigImpl::SettingsImpl::ExtractPolicy::Never)
            return;

        if(config_->referencedDeclarations ==
            ConfigImpl::SettingsImpl::ExtractPolicy::Dependency)
        {
            if(currentMode() != ExtractMode::DirectDependency)
                return;
        }

        // if it was already extracted, we are done
        if(getInfo(id))
            return;

        // FIXME: we need to handle member specializations correctly.
        // we do not want to extract all members of the enclosing
        // implicit instantiation
        Decl* Outer = D;
        DeclContext* DC = D->getDeclContext();
        do
        {
            if(DC->isFileContext() ||
                DC->isFunctionOrMethod())
                break;
            // when extracting dependencies, we want to extract
            // all members of the containing class, not just this one
            if(auto* TD = dyn_cast<TagDecl>(DC))
                Outer = TD;
        }
        while((DC = DC->getParent()));

        if(TemplateDecl* TD = Outer->getDescribedTemplate())
            Outer = TD;

        // add the adjusted declaration to the set of dependencies
        if(! isa<NamespaceDecl, TranslationUnitDecl>(Outer))
            dependencies_.insert(Outer);
    }

    //------------------------------------------------

    /** Generates a USR for a declaration.

        Returns true if USR generation failed,
        and false otherwise.
    */
    bool
    generateUSR(const Decl* D)
    {
        MRDOCS_ASSERT(D);
        MRDOCS_ASSERT(usr_.empty());

        if (const auto* NAD = dyn_cast<NamespaceAliasDecl>(D))
        {
            if (index::generateUSRForDecl(cast<Decl>(NAD->getNamespace()), usr_))
                return true;
            usr_.append("@NA");
            usr_.append(NAD->getNameAsString());
            return false;
        }

        // Handling UsingDirectiveDecl
        if (const auto* UDD = dyn_cast<UsingDirectiveDecl>(D))
        {
            if (index::generateUSRForDecl(UDD->getNominatedNamespace(), usr_)) {
                return true;
            }
            usr_.append("@UD");
            usr_.append(UDD->getQualifiedNameAsString());
            return false;
        }

        // Handling UsingDecl
        if (const auto* UD = dyn_cast<UsingDecl>(D))
        {
            for (const auto* shadow : UD->shadows())
            {
                if (index::generateUSRForDecl(shadow->getTargetDecl(), usr_))
                    return true;
            }
            usr_.append("@UDec");
            usr_.append(UD->getQualifiedNameAsString());
            return false;
        }

        // Handling UnresolvedUsingTypenameDecl
        if (const auto* UD = dyn_cast<UnresolvedUsingTypenameDecl>(D))
        {
            if (index::generateUSRForDecl(UD, usr_))
                return true;
            usr_.append("@UUTDec");
            usr_.append(UD->getQualifiedNameAsString());
            return false;
        }

        // Handling UnresolvedUsingValueDecl
        if (const auto* UD = dyn_cast<UnresolvedUsingValueDecl>(D))
        {
            if (index::generateUSRForDecl(UD, usr_))
                return true;
            usr_.append("@UUV");
            usr_.append(UD->getQualifiedNameAsString());
            return false;
        }

        // Handling UsingPackDecl
        if (const auto* UD = dyn_cast<UsingPackDecl>(D))
        {
            if (index::generateUSRForDecl(UD, usr_))
                return true;
            usr_.append("@UPD");
            usr_.append(UD->getQualifiedNameAsString());
            return false;
        }

        // Handling UsingEnumDecl
        if (const auto* UD = dyn_cast<UsingEnumDecl>(D))
        {
            if (index::generateUSRForDecl(UD, usr_))
                return true;
            usr_.append("@UED");
            EnumDecl const* ED = UD->getEnumDecl();
            usr_.append(ED->getQualifiedNameAsString());
            return false;
        }

        // KRYSTIAN NOTE: clang doesn't currently support
        // generating USRs for friend declarations, so we
        // will improvise until I can merge a patch which
        // adds support for them
        if(const auto* FD = dyn_cast<FriendDecl>(D))
        {
            // first, generate the USR for the containing class
            if(index::generateUSRForDecl(
                cast<Decl>(FD->getDeclContext()), usr_))
                return true;
            // add a seperator for uniqueness
            usr_.append("@FD");
            // if the friend declaration names a type,
            // use the USR generator for types
            if(TypeSourceInfo* TSI = FD->getFriendType())
                return index::generateUSRForType(
                    TSI->getType(), context_, usr_);
            // otherwise, fallthrough and append the
            // USR of the nominated declaration
            if(! (D = FD->getFriendDecl()))
                return true;
        }
        return index::generateUSRForDecl(D, usr_);
    }

    /** Extracts the symbol ID for a declaration.

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

        @return true if the symbol ID could not be
        extracted, and false otherwise.

     */
    bool
    extractSymbolID(
        const Decl* D,
        SymbolID& id)
    {
        if(! D)
            return false;
        if(isa<TranslationUnitDecl>(D))
        {
            id = SymbolID::global;
            return true;
        }
        usr_.clear();
        if(generateUSR(D))
            return false;
        auto h = llvm::SHA1::hash(arrayRefFromStringRef(usr_));
        id = SymbolID(h.data());
        return true;
    }

    /** Extracts the symbol ID for a declaration.

        This function will extract the symbol ID for a
        declaration, and return it.

        As USRs (Unified Symbol Resolution) could be
        large, especially for functions with long type
        arguments, we use 160-bits SHA1(USR) values.

        To guarantee the uniqueness of symbols while using
        a relatively small amount of memory (vs storing
        USRs directly), this function hashes the Decl
        USR value with SHA1.

        @return the symbol ID for the declaration.
     */
    SymbolID
    extractSymbolID(
        const Decl* D)
    {
        SymbolID id = SymbolID::invalid;
        extractSymbolID(D, id);
        return id;
    }

    //------------------------------------------------

    AccessSpecifier
    getAccess(const Decl* D)
    {
        // First, get the declaration this was instantiated from
        D = getInstantiatedFrom(D);

        // If this is the template declaration of a template,
        // use the access of the template
        if(TemplateDecl const* TD = D->getDescribedTemplate())
            return TD->getAccessUnsafe();

        // For class/variable template partial/explicit specializations,
        // we want to use the access of the primary template
        if(auto const* CTSD = dyn_cast<ClassTemplateSpecializationDecl>(D))
            return CTSD->getSpecializedTemplate()->getAccessUnsafe();

        if(auto const* VTSD = dyn_cast<VarTemplateSpecializationDecl>(D))
            return VTSD->getSpecializedTemplate()->getAccessUnsafe();

        // For function template specializations, use the access of the
        // primary template if it has been resolved
        if(auto const* FD = dyn_cast<FunctionDecl>(D))
        {
            if(auto const* FTD = FD->getPrimaryTemplate())
                return FTD->getAccessUnsafe();
        }

        // Since friend declarations are not members, this hack computes
        // their access based on the default access for the tag they
        // appear in, and any AccessSpecDecls which appears lexically
        // before them
        if(auto const* FD = dyn_cast<FriendDecl>(D))
        {
            auto const* RD = dyn_cast<CXXRecordDecl>(
                FD->getLexicalDeclContext());
            // RD should never be null in well-formed code,
            // but clang error recovery may build an AST
            // where the assumption will not hold
            if(! RD)
                return AccessSpecifier::AS_public;
            auto access = RD->isClass() ?
                AccessSpecifier::AS_private :
                AccessSpecifier::AS_public;
            for(auto* M : RD->decls())
            {
                if(auto* AD = dyn_cast<AccessSpecDecl>(M))
                    access = AD->getAccessUnsafe();
                else if(M == FD)
                    return access;
            }
            // KRYSTIAN FIXME: will this ever be hit?
            // it would require a friend declaration that is
            // not in the lexical traversal of its lexical context
            MRDOCS_UNREACHABLE();
        }

        // In all other cases, use the access of this declaration
        return D->getAccessUnsafe();
    }

    //------------------------------------------------

    // KRYSTIAN NOTE: we *really* should not have a
    // type named "SourceLocation"...
    FileInfo* getFileInfo(clang::SourceLocation loc)
    {
        // KRYSTIAN FIXME: we really should not be
        // calling getPresumedLoc this often,
        // it's quite expensive
        auto presumed = source_.getPresumedLoc(loc, false);
        if(presumed.isInvalid())
            return nullptr;
        const FileEntry* file =
            source_.getFileEntryForID(
                presumed.getFileID());
        // KRYSTIAN NOTE: i have no idea under what
        // circumstances the file entry would be null
        if(! file)
            return nullptr;
        // KRYSTIAN NOTE: i have no idea under what
        // circumstances the file would not be in either
        // the main file, or an included file
        auto it = files_.find(file);
        if(it == files_.end())
            return nullptr;
        return &it->second;
    }

    /** Add a source location to an Info object.

        This function will add a source location to an Info,
        if it is not already present.

        @param I the Info to add the source location to
        @param loc the source location to add
        @param definition true if the source location is a definition
        @param documented true if the source location is documented
     */
    void
    addSourceLocation(
        SourceInfo& I,
        clang::SourceLocation loc,
        bool definition,
        bool documented)
    {
        unsigned line = source_.getPresumedLoc(
            loc, false).getLine();
        FileInfo* file = getFileInfo(loc);
        MRDOCS_ASSERT(file);
        if(definition)
        {
            if(I.DefLoc)
                return;
            I.DefLoc.emplace(file->full_path,
                file->short_path, line, file->kind,
                documented);
        }
        else
        {
            auto existing = std::find_if(I.Loc.begin(), I.Loc.end(),
                [line, file](const Location& l)
                {
                    return l.LineNumber == line &&
                        l.Path == file->full_path;
                });
            if(existing != I.Loc.end())
                return;
            I.Loc.emplace_back(file->full_path,
                file->short_path, line, file->kind,
                documented);
        }
    }

    std::string
    getSourceCode(
        SourceRange const& R)
    {
        return Lexer::getSourceText(
            CharSourceRange::getTokenRange(R),
            source_,
            context_.getLangOpts()).str();
    }

    //------------------------------------------------

    std::string getExprAsString(const Expr* E)
    {
        std::string result;
        llvm::raw_string_ostream stream(result);
        E->printPretty(stream, nullptr, context_.getPrintingPolicy());
        return result;
    }

    std::string getTypeAsString(const Type* T)
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
            if(TemplateTypeParmDecl* TTPD = TTPT->getDecl();
                TTPD && TTPD->isImplicit())
                return "auto";
        }
        return QualType(T, 0).getAsString(
            context_.getPrintingPolicy());
    }

    /** Get the user-written `Decl` for a `Decl`

        Given a `Decl` `D`, `getInstantiatedFrom` will return the
        user-written `Decl` corresponding to `D`. For specializations
        which were implicitly instantiated, this will be whichever `Decl`
        was used as the pattern for instantiation.
    */
    template<typename DeclTy>
    DeclTy*
    getInstantiatedFrom(DeclTy* D);

    template<typename DeclTy>
        requires std::derived_from<DeclTy, FunctionDecl> ||
            std::same_as<FunctionTemplateDecl, std::remove_cv_t<DeclTy>>
    FunctionDecl*
    getInstantiatedFrom(DeclTy* D)
    {
        return dyn_cast_if_present<FunctionDecl>(
            getInstantiatedFrom<Decl>(D));
    }

    template<typename DeclTy>
        requires std::derived_from<DeclTy, CXXRecordDecl> ||
            std::same_as<ClassTemplateDecl, std::remove_cv_t<DeclTy>>
    CXXRecordDecl*
    getInstantiatedFrom(DeclTy* D)
    {
        return dyn_cast_if_present<CXXRecordDecl>(
            getInstantiatedFrom<Decl>(D));
    }

    template<typename DeclTy>
        requires std::derived_from<DeclTy, VarDecl> ||
            std::same_as<VarTemplateDecl, std::remove_cv_t<DeclTy>>
    VarDecl*
    getInstantiatedFrom(DeclTy* D)
    {
        return dyn_cast_if_present<VarDecl>(
            getInstantiatedFrom<Decl>(D));
    }

    template<typename DeclTy>
        requires std::derived_from<DeclTy, TypedefNameDecl> ||
            std::same_as<TypeAliasTemplateDecl, std::remove_cv_t<DeclTy>>
    TypedefNameDecl*
    getInstantiatedFrom(DeclTy* D)
    {
        return dyn_cast_if_present<TypedefNameDecl>(
            getInstantiatedFrom<Decl>(D));
    }

    std::unique_ptr<TypeInfo>
    buildTypeInfo(
        QualType qt,
        ExtractMode extract_mode = ExtractMode::IndirectDependency);

    std::unique_ptr<NameInfo>
    buildNameInfo(
        const NestedNameSpecifier* NNS,
        ExtractMode extract_mode = ExtractMode::IndirectDependency);

    std::unique_ptr<NameInfo>
    buildNameInfo(
        const Decl* D,
        ExtractMode extract_mode = ExtractMode::IndirectDependency);


    template<typename Integer>
    Integer
    getValue(const llvm::APInt& V)
    {
        if constexpr(std::is_signed_v<Integer>)
            return static_cast<Integer>(
                V.getSExtValue());
        else
            return static_cast<Integer>(
                V.getZExtValue());
    }

    void
    buildNoexceptInfo(
        NoexceptInfo& I,
        const FunctionProtoType* FPT)
    {
        MRDOCS_ASSERT(FPT);
        I.Implicit = ! FPT->hasNoexceptExceptionSpec();
        #if 0
        // if the exception specification is unevaluated,
        // we just consider it to be dependent
        if(FPT->getExceptionSpecType() ==
            ExceptionSpecificationType::EST_Unevaluated)
            I.Kind = NoexceptKind::Dependent;
        else
            I.Kind = convertToNoexceptKind(FPT->canThrow());
        #else
        I.Kind = convertToNoexceptKind(
            FPT->getExceptionSpecType());
        #endif

        // store the operand, if any
        if(Expr* NoexceptExpr = FPT->getNoexceptExpr())
            I.Operand = getExprAsString(NoexceptExpr);
    }

    void
    buildExprInfo(
        ExprInfo& I,
        const Expr* E)
    {
        if(! E)
            return;
        I.Written = getSourceCode(
            E->getSourceRange());
    }

    template<typename T>
    void
    buildExprInfo(
        ConstantExprInfo<T>& I,
        const Expr* E)
    {
        buildExprInfo(
            static_cast<ExprInfo&>(I), E);
        // if the expression is dependent,
        // we cannot get its value
        if(! E || E->isValueDependent())
            return;
        I.Value.emplace(getValue<T>(
            E->EvaluateKnownConstInt(context_)));
    }

    template<typename T>
    void
    buildExprInfo(
        ConstantExprInfo<T>& I,
        const Expr* E,
        const llvm::APInt& V)
    {
        buildExprInfo(I, E);
        I.Value.emplace(getValue<T>(V));
    }

    QualType
    getDeclaratorType(
        const DeclaratorDecl* DD)
    {
        if(auto* TSI = DD->getTypeSourceInfo();
            TSI && ! TSI->getType().isNull())
            return TSI->getType();
        return DD->getType();
    }

    std::unique_ptr<TParam>
    buildTemplateParam(
        const NamedDecl* N)
    {
        auto TP = visit(N, [&]<typename DeclTy>(
            const DeclTy* P) ->
                std::unique_ptr<TParam>
        {
            constexpr Decl::Kind kind =
                DeclToKind<DeclTy>();

            if constexpr(kind == Decl::TemplateTypeParm)
            {
                auto R = std::make_unique<TypeTParam>();
                if(P->wasDeclaredWithTypename())
                    R->KeyKind = TParamKeyKind::Typename;
                if(P->hasDefaultArgument())
                    R->Default = buildTemplateArg(
                        P->getDefaultArgument().getArgument());
                return R;
            }
            else if constexpr(kind == Decl::NonTypeTemplateParm)
            {
                auto R = std::make_unique<NonTypeTParam>();
                R->Type = buildTypeInfo(P->getType());
                if(P->hasDefaultArgument())
                    R->Default = buildTemplateArg(
                        P->getDefaultArgument().getArgument());
                return R;
            }
            else if constexpr(kind == Decl::TemplateTemplateParm)
            {
                auto R = std::make_unique<TemplateTParam>();
                for(const NamedDecl* NP : *P->getTemplateParameters())
                    R->Params.emplace_back(
                        buildTemplateParam(NP));

                if(P->hasDefaultArgument())
                    R->Default = buildTemplateArg(
                        P->getDefaultArgument().getArgument());
                return R;
            }
            MRDOCS_UNREACHABLE();
        });

        TP->Name = extractName(N);
        // KRYSTIAN NOTE: Decl::isParameterPack
        // returns true for function parameter packs
        TP->IsParameterPack =
            N->isTemplateParameterPack();

        return TP;
    }

    void
    buildTemplateParams(
        TemplateInfo& I,
        const TemplateParameterList* TPL)
    {
        for(const NamedDecl* ND : *TPL)
        {
            I.Params.emplace_back(
                buildTemplateParam(ND));
        }
    }

    std::unique_ptr<TArg>
    buildTemplateArg(
        const TemplateArgument& A)
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
            R->Type = buildTypeInfo(QT);
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
            TemplateName TN = A.getAsTemplateOrTemplatePattern();
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
                    getDependencyID(getInstantiatedFrom<
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

    template<typename Range>
    void
    buildTemplateArgs(
        std::vector<std::unique_ptr<TArg>>& result,
        Range&& args)
    {
        for(const TemplateArgument& arg : args)
        {
            // KRYSTIAN NOTE: is this correct? should we have a
            // separate TArgKind for packs instead of "unlaminating"
            // them as we are doing here?
            if(arg.getKind() == TemplateArgument::Pack)
                buildTemplateArgs(result, arg.pack_elements());
            else
                result.emplace_back(buildTemplateArg(arg));
        }
    }

    void
    buildTemplateArgs(
        std::vector<std::unique_ptr<TArg>>& result,
        const ASTTemplateArgumentListInfo* args)
    {
        return buildTemplateArgs(result,
            std::views::transform(args->arguments(),
                [](auto& x) -> auto&
            {
                    return x.getArgument();
            }));
    }

    /** Parse the comments above a declaration as Javadoc

        This function will parse the comments above a declaration
        as Javadoc, and store the results in the `javadoc` input
        parameter.

        @return true if the comments were successfully parsed as
        Javadoc, and false otherwise.
     */
    bool
    parseRawComment(
        std::unique_ptr<Javadoc>& javadoc,
        Decl const* D)
    {
        // VFALCO investigate whether we can use
        // ASTContext::getCommentForDecl instead
        #if 1
        RawComment* RC =
            D->getASTContext().getRawCommentForDeclNoCache(D);
        if(! RC)
            return false;
        comments::FullComment* FC =
            RC->parse(D->getASTContext(), &sema_.getPreprocessor(), D);
        #else
        comments::FullComment* FC =
            D->getASTContext().getCommentForDecl(
                D, &sema_.getPreprocessor());
        #endif
        if(! FC)
            return false;
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

    //------------------------------------------------

    bool
    checkSymbolFilter(const NamedDecl* ND)
    {
        if(currentMode() != ExtractMode::Normal ||
            symbolFilter_.detached)
            return true;

        std::string name = extractName(ND);
        const FilterNode* parent = symbolFilter_.current;
        if(const FilterNode* child = parent->findChild(name))
        {
            // if there is a matching node, skip extraction if it's
            // explicitly excluded AND has no children. the presence
            // of child nodes indicates that some child exists that
            // is explicitly whitelisted
            if(child->Explicit && child->Excluded &&
                child->isTerminal())
                return false;
            symbolFilter_.setCurrent(child, false);
        }
        else
        {
            // if there was no matching node, check the most
            // recently entered explicitly specified parent node.
            // if it's blacklisted, then the "filtering default"
            // is to exclude symbols unless a child is explicitly
            // whitelisted
            if(symbolFilter_.last_explicit &&
                symbolFilter_.last_explicit->Excluded)
                return false;

            if(const auto* DC = dyn_cast<DeclContext>(ND);
                ! DC || ! DC->isInlineNamespace())
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
                    if(isa<TranslationUnitDecl>(P))
                        break;
                    parents.push_back(cast<NamedDecl>(P));
                }

                // check whether each parent passes the symbol filters
                // as-if the declaration was inline
                for(const auto* PND : std::views::reverse(parents))
                {
                    if(! checkSymbolFilter(PND))
                        return false;
                }
            }

            if(! checkSymbolFilter(ND))
                return false;
        }

        FileInfo* file = getFileInfo(D->getBeginLoc());
        // KRYSTIAN NOTE: i'm unsure under what conditions
        // this assert would fire.
        MRDOCS_ASSERT(file);
        // only extract from files in source root
        return file->kind == FileKind::Source;
    }

    /** Determine if a declaration should be extracted

        This function will determine whether a declaration
        should be extracted based on the current extraction
        mode, and the current symbol filter state.

        The function filters private symbols, symbols outside
        the input files, and symbols in files that do not match
        the input file patterns.

        @param D the declaration to check
        @param access the access specifier of the declaration
        @return true if the declaration should be extracted,
        and false otherwise.
     */
    bool
    shouldExtract(
        const Decl* D,
        AccessSpecifier access)
    {
        if (config_->inaccessibleMembers !=
            ConfigImpl::SettingsImpl::ExtractPolicy::Always)
        {
            // KRYSTIAN FIXME: this doesn't handle direct
            // dependencies on inaccessible declarations
            if(access == AccessSpecifier::AS_private ||
                access == AccessSpecifier::AS_protected)
                return false;
        }

        if (!config_->input.include.empty())
        {
            // Get filename
            FileInfo* file = getFileInfo(D->getBeginLoc());
            if (!file)
                return false;
            std::string filename = file->full_path;
            bool matchPrefix = std::ranges::any_of(
                config_->input.include,
                [&filename](const std::string& prefix)
                {
                    return filename.starts_with(prefix);
                });
            if (!matchPrefix)
            {
                return false;
            }
        }

        if (!config_->input.filePatterns.empty())
        {
            // Get filename
            FileInfo* file = getFileInfo(D->getBeginLoc());
            if (!file)
                return false;
            std::string filename = file->full_path;
            bool matchPattern = std::ranges::any_of(
                config_->input.filePatterns,
                [&filename](const std::string& pattern)
                {
                    return globMatch(pattern, filename);
                });
            if (!matchPattern)
            {
                return false;
            }
        }

    #if 0
        bool extract = inExtractedFile(D);
        // if we're extracting a declaration as a dependency,
        // override the current extraction mode if
        // it would be extracted anyway
        if(extract)
            mode = ExtractMode::Normal;

        return extract || currentMode() != ExtractMode::Normal;
    #else
        return inExtractedFile(D) ||
            currentMode() != ExtractMode::Normal;
    #endif
    }

    std::string
    extractName(
        const NamedDecl* D)
    {
        std::string result;
        DeclarationName N = D->getDeclName();
        switch(N.getNameKind())
        {
        case DeclarationName::Identifier:
            if(const auto* I = N.getAsIdentifierInfo())
                result.append(I->getName());
            break;
        case DeclarationName::CXXDestructorName:
            result.push_back('~');
            [[fallthrough]];
        case DeclarationName::CXXConstructorName:
            if(const auto* R = N.getCXXNameType()->getAsCXXRecordDecl())
                result.append(R->getIdentifier()->getName());
            break;
        case DeclarationName::CXXDeductionGuideName:
            if(const auto* T = N.getCXXDeductionGuideTemplate())
                result.append(T->getIdentifier()->getName());
            break;
        case DeclarationName::CXXConversionFunctionName:
        {
            MRDOCS_ASSERT(isa<CXXConversionDecl>(D));
            const auto* CD = cast<CXXConversionDecl>(D);
            result.append("operator ");
            // KRYSTIAN FIXME: we *really* should not be
            // converting types to strings like this
            result.append(toString(
                *buildTypeInfo(
                    CD->getReturnType())));
            break;
        }
        case DeclarationName::CXXOperatorName:
        {
            OperatorKind K = convertToOperatorKind(
                N.getCXXOverloadedOperator());
            result.append("operator");
            std::string_view name = getOperatorName(K);
            if(std::isalpha(name.front()))
                result.push_back(' ');
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

    //------------------------------------------------

    const Decl*
    getParentDecl(const Decl* D)
    {
        return getParentDecl(const_cast<Decl*>(D));
    }

    Decl*
    getParentDecl(Decl* D)
    {
        while((D = cast_if_present<
            Decl>(D->getDeclContext())))
        {
            switch(D->getKind())
            {
            case Decl::TranslationUnit:
            case Decl::Namespace:
            case Decl::Enum:
            case Decl::CXXRecord:
            case Decl::ClassTemplateSpecialization:
            case Decl::ClassTemplatePartialSpecialization:
                return D;
            // we consider all other DeclContexts to be "transparent"
            default:
                break;
            }
        }
        return nullptr;
    }

    /** Populate the Info with its parent namespaces

        Given a Decl `D`, `getParentNamespaces` will populate
        the `Info` `I` with the SymbolID of each parent namespace
        of `D`. The SymbolID of the global namespace is always
        included as the first element of `I.Namespace`.

        @param I The mrdocs Info to populate
        @param D The Decl to extract
     */
    void
    getParentNamespaces(
        Info& I,
        Decl* D)
    {
        Decl* PD = getParentDecl(D);
        SymbolID ParentID = extractSymbolID(PD);
        switch(PD->getKind())
        {
        // The TranslationUnit DeclContext is the global namespace;
        // it uses SymbolID::global and should *always* exist
        case Decl::TranslationUnit:
        {
            MRDOCS_ASSERT(ParentID == SymbolID::global);
            auto [P, created] = getOrCreateInfo<
                NamespaceInfo>(ParentID);
            emplaceChild(P, I);
            break;
        }
        case Decl::Namespace:
        {
            auto [P, created] = getOrCreateInfo<
                NamespaceInfo>(ParentID);
            buildNamespace(P, created, cast<NamespaceDecl>(PD));
            emplaceChild(P, I);
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

            auto [P, created] = getOrCreateInfo<
                SpecializationInfo>(ParentID);
            buildSpecialization(P, created, S);
            emplaceChild(P, I);
            break;
        }
        // non-implicit instantiations should be
        // treated like normal CXXRecordDecls
        [[fallthrough]];
        // we should never encounter a Record
        // that is not a CXXRecord
        case Decl::CXXRecord:
        {
            auto [P, created] = getOrCreateInfo<
                RecordInfo>(ParentID);
            buildRecord(P, created, cast<CXXRecordDecl>(PD));
            emplaceChild(P, I);
            break;
        }
        case Decl::Enum:
        {
            auto [P, created] = getOrCreateInfo<
                EnumInfo>(ParentID);
            buildEnum(P, created, cast<EnumDecl>(PD));
            emplaceChild(P, I);
            break;
        }
        default:
            MRDOCS_UNREACHABLE();
        }

        Info* P = getInfo(ParentID);
        MRDOCS_ASSERT(P);

        I.Namespace.emplace_back(ParentID);
        I.Namespace.insert(I.Namespace.end(),
            P->Namespace.begin(), P->Namespace.end());
    }

    /** Emplace a child Info into a ScopeInfo

        Given a ScopeInfo `P` and an Info `C`, `emplaceChild` will
        add the SymbolID of `C` to `P.Members` and `P.Lookups[C.Name]`.

        @param P The parent ScopeInfo
        @param C The child Info
     */
    void
    emplaceChild(
        ScopeInfo& P,
        Info& C)
    {
        // Include C.id in P.Members if it's not already there
        if(std::ranges::find(P.Members, C.id) == P.Members.end())
            P.Members.emplace_back(C.id);

        // Include C.id in P.Lookups[C.Name] if it's not already there
        auto& lookups = P.Lookups.try_emplace(C.Name).first->second;
        if(std::ranges::find(lookups, C.id) == lookups.end())
            lookups.emplace_back(C.id);
    }

    //------------------------------------------------

    void
    buildSpecialization(
        SpecializationInfo& I,
        bool created,
        ClassTemplateSpecializationDecl* D)
    {
        if(! created)
            return;

        CXXRecordDecl* PD = getInstantiatedFrom(D);

        buildTemplateArgs(I.Args,
            D->getTemplateArgs().asArray());

        extractSymbolID(PD, I.Primary);
        I.Name = extractName(PD);

        getParentNamespaces(I, D);
    }

    //------------------------------------------------
    // Decl types which have isThisDeclarationADefinition:
    //
    // VarTemplateDecl
    // FunctionTemplateDecl
    // FunctionDecl
    // TagDecl
    // ClassTemplateDecl
    // CXXDeductionGuideDecl

    /** Populate the NamespaceInfo with the content of a NamespaceDecl

        @param I The mrdocs NamespaceInfo to populate
        @param created Whether the NamespaceInfo was just created
        @param D The NamespaceDecl to extract
     */
    void
    buildNamespace(
        NamespaceInfo& I,
        bool created,
        NamespaceDecl* D)
    {
        if(! created)
            // Namespace already extracted:
            // nothing to populate
            return;

        // KRYSTIAN NOTE: we do not extract
        // javadocs for namespaces
        if(D->isAnonymousNamespace())
            I.specs.isAnonymous = true;
        else
            I.Name = extractName(D);
        I.specs.isInline = D->isInline();

        getParentNamespaces(I, D);
    }

    //------------------------------------------------

    void
    buildRecord(
        RecordInfo& I,
        bool created,
        CXXRecordDecl* D)
    {
        bool documented = parseRawComment(I.javadoc, D);
        addSourceLocation(I, D->getBeginLoc(),
            D->isThisDeclarationADefinition(), documented);

        if(! created)
            return;

        NamedDecl* ND = D;
        if(TypedefNameDecl* TD =
            D->getTypedefNameForAnonDecl())
        {
            I.IsTypeDef = true;
            ND = TD;
        }
        I.Name = extractName(ND);

        I.KeyKind = convertToRecordKeyKind(D->getTagKind());

        // These are from CXXRecordDecl::isEffectivelyFinal()
        I.specs.isFinal = D->template hasAttr<FinalAttr>();
        if(const auto* DT = D->getDestructor())
            I.specs.isFinalDestructor = DT->template hasAttr<FinalAttr>();

        // extract direct bases. D->bases() will get the bases
        // from whichever declaration is the definition (if any)
        if(D->hasDefinition())
        {
            for(const CXXBaseSpecifier& B : D->bases())
            {
                AccessSpecifier access = B.getAccessSpecifier();
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
                auto BaseType = buildTypeInfo(
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

        getParentNamespaces(I, D);
    }

    //------------------------------------------------

    void
    buildEnum(
        EnumInfo& I,
        bool created,
        EnumDecl* D)
    {
        bool documented = parseRawComment(I.javadoc, D);
        addSourceLocation(I, D->getBeginLoc(),
            D->isThisDeclarationADefinition(), documented);

        if(! created)
            return;

        I.Name = extractName(D);

        I.Scoped = D->isScoped();

        if(D->isFixed())
            I.UnderlyingType = buildTypeInfo(
                D->getIntegerType());

        getParentNamespaces(I, D);
    }

    //------------------------------------------------

    void
    buildEnumerator(
        EnumeratorInfo& I,
        bool created,
        EnumConstantDecl* D)
    {
        bool documented = parseRawComment(I.javadoc, D);
        addSourceLocation(I, D->getBeginLoc(), true, documented);

        if(! created)
            return;

        I.Name = extractName(D);

        buildExprInfo(
            I.Initializer,
            D->getInitExpr(),
            D->getInitVal());

        getParentNamespaces(I, D);
    }

    //------------------------------------------------

    void
    buildTypedef(
        TypedefInfo& I,
        bool created,
        TypedefNameDecl* D)
    {
        bool documented = parseRawComment(I.javadoc, D);
        // KRYSTIAN FIXME: we currently treat typedef/alias
        // declarations as having a single definition; however,
        // such declarations are never definitions and can
        // be redeclared multiple times (even in the same scope)
        addSourceLocation(I, D->getBeginLoc(), true, documented);

        if(! created)
            return;

        I.Name = extractName(D);

        // When a symbol has a dependency on a typedef, we also
        // consider the symbol to have a dependency on the aliased
        // type. Therefore, we propagate the current dependency mode
        // when building the TypeInfo for the aliased type
        I.Type = buildTypeInfo(
            D->getUnderlyingType(),
            currentMode());

    #if 0
        if(I.Type.Name.empty())
        {
            // Typedef for an unnamed type. This is like
            // "typedef struct { } Foo;". The record serializer
            // explicitly checks for this syntax and constructs
            // a record with that name, so we don't want to emit
            // a duplicate here.
            return;
        }
    #endif

        getParentNamespaces(I, D);
    }

    //------------------------------------------------

    void
    buildVariable(
        VariableInfo& I,
        bool created,
        VarDecl* D)
    {
        bool documented = parseRawComment(I.javadoc, D);
        addSourceLocation(I, D->getBeginLoc(),
            D->isThisDeclarationADefinition(), documented);

        // KRYSTIAN FIXME: we need to properly merge storage class
        I.specs.storageClass |=
            convertToStorageClassKind(
                D->getStorageClass());

        // this handles thread_local, as well as the C
        // __thread and __Thread_local specifiers
        I.specs.isThreadLocal |= D->getTSCSpec() !=
            ThreadStorageClassSpecifier::TSCS_unspecified;

        // KRYSTIAN NOTE: VarDecl does not provide getConstexprKind,
        // nor does it use getConstexprKind to store whether
        // a variable is constexpr/constinit. Although
        // only one is permitted in a variable declaration,
        // it is possible to declare a static data member
        // as both constexpr and constinit in separate declarations..
        I.specs.isConstinit |= D->hasAttr<ConstInitAttr>();
        if(D->isConstexpr())
            I.specs.constexprKind = ConstexprKind::Constexpr;

        if(const Expr* E = D->getInit())
            buildExprInfo(I.Initializer, E);

        if(! created)
            return;

        I.Name = extractName(D);

        I.Type = buildTypeInfo(D->getType());

        getParentNamespaces(I, D);
    }

    //------------------------------------------------

    void
    buildField(
        FieldInfo& I,
        bool created,
        FieldDecl* D)
    {
        bool documented = parseRawComment(I.javadoc, D);
        // fields (i.e. non-static data members)
        // cannot have multiple declarations
        addSourceLocation(I, D->getBeginLoc(), true, documented);

        if(! created)
            return;

        I.Name = extractName(D);

        I.Type = buildTypeInfo(D->getType());

        I.IsMutable = D->isMutable();

        if(const Expr* E = D->getInClassInitializer())
            buildExprInfo(I.Default, E);

        if(D->isBitField())
        {
            I.IsBitfield = true;
            buildExprInfo(
                I.BitfieldWidth,
                D->getBitWidth());
        }

        I.specs.hasNoUniqueAddress = D->hasAttr<NoUniqueAddressAttr>();
        I.specs.isDeprecated = D->hasAttr<DeprecatedAttr>();
        I.specs.isMaybeUnused = D->hasAttr<UnusedAttr>();

        getParentNamespaces(I, D);
    }

    //------------------------------------------------

    template<class DeclTy>
    void
    buildFunction(
        FunctionInfo& I,
        bool created,
        DeclTy* D)
    {
        bool documented = parseRawComment(I.javadoc, D);
        addSourceLocation(I, D->getBeginLoc(),
            D->isThisDeclarationADefinition(), documented);

        // KRYSTIAN TODO: move other extraction that requires
        // a valid function type here
        if(auto FT = getDeclaratorType(D); ! FT.isNull())
        {
            const auto* FPT = FT->template getAs<FunctionProtoType>();

            buildNoexceptInfo(I.Noexcept, FPT);

            I.specs0.hasTrailingReturn |= FPT->hasTrailingReturn();
        }

        //
        // FunctionDecl
        //
        I.specs0.isVariadic |= D->isVariadic();
        I.specs0.isDefaulted |= D->isDefaulted();
        I.specs0.isExplicitlyDefaulted |= D->isExplicitlyDefaulted();
        I.specs0.isDeleted |= D->isDeleted();
        I.specs0.isDeletedAsWritten |= D->isDeletedAsWritten();
        I.specs0.isNoReturn |= D->isNoReturn();
            // subsumes D->hasAttr<NoReturnAttr>()
            // subsumes D->hasAttr<CXX11NoReturnAttr>()
            // subsumes D->hasAttr<C11NoReturnAttr>()
            // subsumes D->getType()->getAs<FunctionType>()->getNoReturnAttr()
        I.specs0.hasOverrideAttr |= D->template hasAttr<OverrideAttr>();
        I.specs0.constexprKind |=
            convertToConstexprKind(
                D->getConstexprKind());

        I.specs0.overloadedOperator |=
            convertToOperatorKind(
                D->getOverloadedOperator());
        I.specs0.storageClass |=
            convertToStorageClassKind(
                D->getStorageClass());

        I.specs1.isNodiscard |= D->template hasAttr<WarnUnusedResultAttr>();
        I.specs1.isExplicitObjectMemberFunction |= D->hasCXXExplicitFunctionObjectParameter();
        //
        // CXXMethodDecl
        //
        if constexpr(std::derived_from<DeclTy, CXXMethodDecl>)
        {
            I.specs0.isVirtual |= D->isVirtual();
            I.specs0.isVirtualAsWritten |= D->isVirtualAsWritten();
            I.specs0.isPure |= D->isPureVirtual();
            I.specs0.isConst |= D->isConst();
            I.specs0.isVolatile |= D->isVolatile();
            I.specs0.refQualifier |=
                convertToReferenceKind(
                    D->getRefQualifier());
            I.specs0.isFinal |= D->template hasAttr<FinalAttr>();
            //D->isCopyAssignmentOperator()
            //D->isMoveAssignmentOperator()
            //D->isOverloadedOperator();
            //D->isStaticOverloadedOperator();
        }

        //
        // CXXDestructorDecl
        //
        if constexpr(std::derived_from<DeclTy, CXXDestructorDecl>)
        {
        }

        //
        // CXXConstructorDecl
        //
        if constexpr(std::derived_from<DeclTy, CXXConstructorDecl>)
        {
            I.specs1.explicitSpec |=
                convertToExplicitKind(
                    D->getExplicitSpecifier());
        }

        //
        // CXXConversionDecl
        //
        if constexpr(std::derived_from<DeclTy, CXXConversionDecl>)
        {
            I.specs1.explicitSpec |=
                convertToExplicitKind(
                    D->getExplicitSpecifier());
        }

        for(const ParmVarDecl* P : D->parameters())
        {
            auto index = P->getFunctionScopeIndex();
            Param& param = index < I.Params.size() ?
                I.Params[index] : I.Params.emplace_back();
            // KRYSTIAN NOTE: it's not clear what the correct thing
            // to do here is. this will use the longest name seen
            // in any redeclaration
            if(std::string_view name = P->getName();
                name.size() > param.Name.size())
                param.Name = name;

            if(! param.Type)
                param.Type = buildTypeInfo(P->getOriginalType());

            const Expr* default_arg = P->hasUninstantiatedDefaultArg() ?
                P->getUninstantiatedDefaultArg() : P->getInit();
            if(param.Default.empty() && default_arg)
                param.Default = getSourceCode(
                    default_arg->getSourceRange());
        }

        if(! created)
            return;

        I.Name = extractName(D);

        I.Class = convertToFunctionClass(
            D->getDeclKind());

        QualType RT = D->getReturnType();
        ExtractMode next_mode = ExtractMode::IndirectDependency;
        if(auto* AT = RT->getContainedAutoType();
            AT && AT->hasUnnamedOrLocalType())
        {
            next_mode = ExtractMode::DirectDependency;
        }
        // extract the return type in direct dependency mode
        // if it contains a placeholder type which is
        // deduceded as a local class type
        I.ReturnType = buildTypeInfo(RT, next_mode);

        getParentNamespaces(I, D);
    }

    //------------------------------------------------

    void
    buildGuide(
        GuideInfo& I,
        bool created,
        CXXDeductionGuideDecl* D)
    {
        bool documented = parseRawComment(I.javadoc, D);
        addSourceLocation(I, D->getBeginLoc(), true, documented);

        // deduction guides cannot be redeclared, so there is nothing to merge
        if(! created)
            return;

        I.Name = extractName(D->getDeducedTemplate());

        I.Deduced = buildTypeInfo(D->getReturnType());

        for(const ParmVarDecl* P : D->parameters())
        {
            I.Params.emplace_back(
                buildTypeInfo(P->getOriginalType()),
                P->getNameAsString(),
                // deduction guides cannot have default arguments
                std::string());
        }

        I.Explicit = convertToExplicitKind(
            D->getExplicitSpecifier());

        getParentNamespaces(I, D);
    }

    //------------------------------------------------

    void
    buildFriend(
        FriendInfo& I,
        bool created,
        FriendDecl* D)
    {
        bool documented = parseRawComment(I.javadoc, D);
        addSourceLocation(I, D->getBeginLoc(), true, documented);

        if(! created)
            return;

        // A NamedDecl nominated by a FriendDecl
        // will be one of the following:
        // - FunctionDecl
        // - FunctionTemplateDecl
        // - ClassTemplateDecl
        if(NamedDecl* ND = D->getFriendDecl())
        {
            extractSymbolID(ND, I.FriendSymbol);
            // If this is a friend function declaration naming
            // a previously undeclared function, traverse it.
            // in addition to this, traverse the declaration if
            // it's a class templates first declared as a friend
            if((ND->isFunctionOrFunctionTemplate() &&
                ND->getFriendObjectKind() == Decl::FOK_Undeclared) ||
                (isa<ClassTemplateDecl>(ND) && ND->isFirstDecl()))
                traverseDecl(ND);
        }
        // Since a friend declaration which name non-class types
        // will be ignored, a type nominated by a FriendDecl can
        // essentially be anything
        if(TypeSourceInfo* TSI = D->getFriendType())
            I.FriendType = buildTypeInfo(TSI->getType());

        getParentNamespaces(I, D);
    }

    //------------------------------------------------

    void
    buildAlias(
        AliasInfo& I,
        bool created,
        NamespaceAliasDecl* D)
    {
        bool documented = parseRawComment(I.javadoc, D);
        addSourceLocation(I, D->getBeginLoc(), true, documented);

        if(! created)
            return;

        I.Name = extractName(D);
        I.AliasedSymbol = buildNameInfo(D->getAliasedNamespace());

        getParentNamespaces(I, D);
    }


    //------------------------------------------------

    void
    buildUsingDirective(
        UsingInfo& I,
        bool created,
        UsingDirectiveDecl* D)
    {
        bool documented = parseRawComment(I.javadoc, D);
        addSourceLocation(I, D->getBeginLoc(), true, documented);

        if(! created)
            return;

        I.Class = UsingClass::Namespace;

        if (D->getQualifier())
        {
            I.Qualifier = buildNameInfo(D->getQualifier());
        }

        if (NamedDecl* ND = D->getNominatedNamespace())
        {
            I.Name = extractName(ND);
            SymbolID id;
            getDependencyID(ND, id);
            I.UsingSymbols.emplace_back(id);
        }
        getParentNamespaces(I, D);
    }


    //------------------------------------------------

    void
    buildUsingDeclaration(
        UsingInfo& I,
        bool created,
        UsingDecl* D)
    {
        bool documented = parseRawComment(I.javadoc, D);
        addSourceLocation(I, D->getBeginLoc(), true, documented);

        if(! created)
            return;

        I.Name = extractName(D);
        I.Class = UsingClass::Normal;
        I.Qualifier = buildNameInfo(D->getQualifier());

        for (auto const* UDS : D->shadows())
            getDependencyID(UDS->getTargetDecl(), I.UsingSymbols.emplace_back());

        getParentNamespaces(I, D);
    }

    //------------------------------------------------

    /** Get the DeclType as a MrDocs Info object

        The function will get or create the MrDocs Info
        object for the `DeclType` and set the initial
        Access specifier.

        @param D The declaration to extract
     */
    template<typename DeclType>
    Expected<
        std::pair<
            MrDocsType_t<DeclType>&,
            bool>>
    getAsMrDocsInfo(DeclType* D)
    {
        AccessSpecifier access = getAccess(D);
        MRDOCS_CHECK_MSG(
            shouldExtract(D, access),
            "Symbol should not be extracted");

        SymbolID id;
        MRDOCS_CHECK_MSG(
            extractSymbolID(D, id),
            "Failed to extract symbol ID");

        auto [I, created] = getOrCreateInfo<MrDocsType_t<DeclType>>(id);
        I.Access = convertToAccessKind(access);
        return std::make_pair(std::ref(I), created);
    }


    /** Traverse a C++ namespace declaration

        This function is called by traverseDecl to traverse a
        namespace declaration.

        A NamespaceDecl inherits from NamedDecl.

     */
    void
    traverse(NamespaceDecl*);

    /** Traverse an enum declaration

        This function is called by traverseDecl to traverse an
        enum declaration.

        An EnumDecl inherits from TagDecl.

     */
    void
    traverse(EnumDecl*);

    /** Traverse an enum constant

        This function is called by traverseDecl to traverse an
        enum constant.

        An EnumConstantDecl inherits from ValueDecl.

     */
    void
    traverse(EnumConstantDecl*);

    /** Traverse a friend declaration

        This function is called by traverseDecl to traverse
        a friend declaration.

        A FriendDecl inherits from Decl.

    */
    void
    traverse(FriendDecl*);


    /** Traverse a namespace alias declaration

        This function is called by traverseDecl to traverse
        a namespace alias declaration.

        A NamespaceAliasDecl inherits from NamedDecl.

    */
    void
    traverse(NamespaceAliasDecl*);

    /** Traverse a using directive

        This function is called by traverseDecl to traverse
        a using directive.

        A UsingDirectiveDecl inherits from NamedDecl.

    */
    void
    traverse(UsingDirectiveDecl*);

    /** Traverse a using declaration

        This function is called by traverseDecl to traverse
        a using declaration.

        A UsingDecl inherits from NamedDecl.

    */
    void
    traverse(UsingDecl*);

    /** Traverse a member of a struct, union, or class

        This function is called by traverseDecl to traverse
        a member of a struct, union, or class.

        A FieldDecl inherits from DeclaratorDecl.

     */
    void
    traverse(FieldDecl*);

    /** Traverse a deduction guide

        This function is called by traverseDecl to traverse a
        C++ deduction guide.

        Deduction guides inherit from FunctionDecl.

    */
    void
    traverse(CXXDeductionGuideDecl*, FunctionTemplateDecl* = nullptr);

    /** Traverse a C++ struct, union, or class

        This function is called by traverseDecl to traverse a
        C++ struct, union, or class.

    */
    template<std::derived_from<CXXRecordDecl> CXXRecordTy>
    void
    traverse(CXXRecordTy*, ClassTemplateDecl* = nullptr);

    /** Traverse a variable declaration or definition

        This function is called by traverseDecl to traverse a
        variable declaration or definition.

    */
    template<std::derived_from<VarDecl> VarTy>
    void
    traverse(VarTy*, VarTemplateDecl* = nullptr);

    /** Traverse a function  declaration or definition

        This function is called by traverseDecl to traverse a
        typedef declaration.

    */
    template<std::derived_from<FunctionDecl> FunctionTy>
    void
    traverse(FunctionTy*, FunctionTemplateDecl* = nullptr);

    /** Traverse a typedef declaration

        This function is called by traverseDecl to traverse a
        typedef declaration.

    */
    template<std::derived_from<TypedefNameDecl> TypedefNameTy>
    void
    traverse(TypedefNameTy*, TypeAliasTemplateDecl* = nullptr);

#if 0
    // includes both linkage-specification forms in [dcl.link]:
    //     extern string-literal { declaration-seq(opt) }
    //     extern string-literal name-declaration
    void traverse(LinkageSpecDecl*);
    void traverse(ExternCContextDecl*);
    void traverse(ExportDecl*);
#endif

    /** Traverse any declaration

        Catch-all function so overload resolution does not
        cause a hard error in the Traverse function for Decl.

        The function will attempt to convert the declaration
        to a DeclContext and call traverseContext if
        successful.

     */
    template<typename... Args>
    void
    traverse(Decl* D, Args&&...);

    /** Traverse a declaration context

        This function is called to traverse the members of a declaration
        context.

        The build() function will call this function with
        traverseDecl(context_.getTranslationUnitDecl()) to initiate
        the traversal of the entire AST.

        The function traverseContext(DeclContext* DC) will
        also call this function to traverse each member of
        the declaration context.

     */
    template<typename... Args>
    void
    traverseDecl(Decl* D, Args&&... args);

    /** Traverse declaration contexts

        This function is called to traverse the members of a declaration
        context. These decls are:

        * TranslationUnitDecl
        * ExternCContext
        * NamespaceDecl
        * TagDecl
        * OMPDeclareReductionDecl
        * OMPDeclareMapperDecl
        * FunctionDecl
        * ObjCMethodDecl
        * ObjCContainerDecl
        * LinkageSpecDecl
        * ExportDecl
        * BlockDecl
        * CapturedDecl

        The function will call traverseDecl for each member of the
        declaration context.

    */
    void
    traverseContext(DeclContext* DC);
};

//------------------------------------------------
// NamespaceDecl

void
ASTVisitor::
traverse(NamespaceDecl* D)
{
    if(! shouldExtract(D, AccessSpecifier::AS_none))
        return;

    if(D->isAnonymousNamespace() &&
        config_->anonymousNamespaces !=
            ConfigImpl::SettingsImpl::ExtractPolicy::Always)
    {
        // Always skip anonymous namespaces if so configured
        if(config_->anonymousNamespaces ==
            ConfigImpl::SettingsImpl::ExtractPolicy::Never)
            return;

        // Otherwise, skip extraction if this isn't a dependency
        // KRYSTIAN FIXME: is this correct? a namespace should not
        // be extracted as a dependency (until namespace aliases and
        // using directives are supported)
        if(currentMode() == ExtractMode::Normal)
            return;
    }

    SymbolID id;
    if(! extractSymbolID(D, id))
        return;
    auto [I, created] = getOrCreateInfo<NamespaceInfo>(id);

    buildNamespace(I, created, D);
    traverseContext(D);
}

//------------------------------------------------
// EnumDecl

void
ASTVisitor::
traverse(EnumDecl* D)
{
    auto exp = getAsMrDocsInfo(D);
    if(! exp) { return; }
    auto [I, created] = *exp;
    buildEnum(I, created, D);
    traverseContext(D);
}

//------------------------------------------------
// FieldDecl

void
ASTVisitor::
traverse(FieldDecl* D)
{
    auto exp = getAsMrDocsInfo(D);
    if(! exp) { return; }
    auto [I, created] = *exp;
    buildField(I, created, D);
}

//------------------------------------------------
// EnumConstantDecl

void
ASTVisitor::
traverse(EnumConstantDecl* D)
{
    auto exp = getAsMrDocsInfo(D);
    if(! exp) { return; }
    auto [I, created] = *exp;
    buildEnumerator(I, created, D);
}

//------------------------------------------------
// FriendDecl

void
ASTVisitor::
traverse(FriendDecl* D)
{
    auto exp = getAsMrDocsInfo(D);
    if(! exp) { return; }
    auto [I, created] = *exp;
    buildFriend(I, created, D);
}

//------------------------------------------------
// NamespaceAliasDecl

void
ASTVisitor::
traverse(NamespaceAliasDecl* D)
{
    auto exp = getAsMrDocsInfo(D);
    if(! exp) { return; }
    auto [I, created] = *exp;
    buildAlias(I, created, D);
}

//------------------------------------------------
// UsingDirectiveDecl

void
ASTVisitor::
traverse(UsingDirectiveDecl* D)
{
    auto exp = getAsMrDocsInfo(D);
    if(! exp) { return; }
    auto [I, created] = *exp;
    buildUsingDirective(I, created, D);
}


//------------------------------------------------
// UsingDecl

void
ASTVisitor::
traverse(UsingDecl* D)
{
    auto const exp = getAsMrDocsInfo(D);
    if(! exp) { return; }
    auto [I, created] = *exp;
    buildUsingDeclaration(I, created, D);
}

//------------------------------------------------

template<std::derived_from<CXXRecordDecl> CXXRecordTy>
void
ASTVisitor::
traverse(
    CXXRecordTy* D,
    ClassTemplateDecl* CTD)
{
    auto exp = getAsMrDocsInfo(D);
    if(! exp) { return; }
    auto [I, created] = *exp;

    // CTD is the specialized template if D is a partial or
    // explicit specialization, and the described template otherwise
    if(CTD)
    {
        auto& Template = I.Template = std::make_unique<TemplateInfo>();
        // if D is a partial/explicit specialization, extract the template arguments
        if(auto* CTSD = dyn_cast<ClassTemplateSpecializationDecl>(D))
        {
            extractSymbolID(getInstantiatedFrom(CTD), Template->Primary);
            // extract the template arguments of the specialization
            buildTemplateArgs(Template->Args, CTSD->getTemplateArgsAsWritten());
            // extract the template parameters if this is a partial specialization
            if(auto* CTPSD = dyn_cast<ClassTemplatePartialSpecializationDecl>(D))
                buildTemplateParams(*I.Template, CTPSD->getTemplateParameters());
        }
        else
        {
            // otherwise, extract the template parameter list from CTD
            buildTemplateParams(*I.Template, CTD->getTemplateParameters());
        }
    }

    buildRecord(I, created, D);
    traverseContext(D);
}

template<std::derived_from<VarDecl> VarTy>
void
ASTVisitor::
traverse(
    VarTy* D,
    VarTemplateDecl* VTD)
{
    auto exp = getAsMrDocsInfo(D);
    if(! exp) { return; }
    auto [I, created] = *exp;

    // VTD is the specialized template if D is a partial or
    // explicit specialization, and the described template otherwise
    if(VTD)
    {
        auto& Template = I.Template = std::make_unique<TemplateInfo>();
        // if D is a partial/explicit specialization, extract the template arguments
        if(auto* VTSD = dyn_cast<VarTemplateSpecializationDecl>(D))
        {
            extractSymbolID(getInstantiatedFrom(VTD), Template->Primary);
            // extract the template arguments of the specialization
            buildTemplateArgs(Template->Args, VTSD->getTemplateArgsAsWritten());
            // extract the template parameters if this is a partial specialization
            if(auto* VTPSD = dyn_cast<VarTemplatePartialSpecializationDecl>(D))
                buildTemplateParams(*I.Template, VTPSD->getTemplateParameters());
        }
        else
        {
            // otherwise, extract the template parameter list from VTD
            buildTemplateParams(*I.Template, VTD->getTemplateParameters());
        }
    }

    buildVariable(I, created, D);
}

void
ASTVisitor::
traverse(
    CXXDeductionGuideDecl* D,
    FunctionTemplateDecl* FTD)
{
    auto exp = getAsMrDocsInfo(D);
    if(! exp) { return; }
    auto [I, created] = *exp;

    // D is the templated declaration if FTD is non-null
    if(FTD)
    {
        auto& Template = I.Template = std::make_unique<TemplateInfo>();
        buildTemplateParams(*Template, FTD->getTemplateParameters());
    }

    buildGuide(I, created, D);
}

template<std::derived_from<FunctionDecl> FunctionTy>
void
ASTVisitor::
traverse(
    FunctionTy* D,
    FunctionTemplateDecl* FTD)
{
    auto exp = getAsMrDocsInfo(D);
    if(! exp) { return; }
    auto [I, created] = *exp;

    // D is the templated declaration if FTD is non-null
    if(FTD || D->isFunctionTemplateSpecialization())
    {
        auto& Template = I.Template = std::make_unique<TemplateInfo>();

        if(auto* FTSI = D->getTemplateSpecializationInfo())
        {
            extractSymbolID(getInstantiatedFrom(
                FTSI->getTemplate()), Template->Primary);
            // TemplateArguments is used instead of TemplateArgumentsAsWritten
            // because explicit specializations of function templates may have
            // template arguments deduced from their return type and parameters
            if(auto* Args = FTSI->TemplateArguments)
                buildTemplateArgs(Template->Args, Args->asArray());
        }
        else if(auto* DFTSI = D->getDependentSpecializationInfo())
        {
            // Only extract the ID of the primary template if there is
            // a single candidate primary template.
            if(auto Candidates = DFTSI->getCandidates(); Candidates.size() == 1)
                extractSymbolID(getInstantiatedFrom(
                    Candidates.front()), Template->Primary);
            if(auto* Args = DFTSI->TemplateArgumentsAsWritten)
                buildTemplateArgs(Template->Args, Args);
        }
        else
        {
            buildTemplateParams(*Template, FTD->getTemplateParameters());
        }
    }

    if constexpr(std::same_as<FunctionTy, CXXDeductionGuideDecl>)
        buildGuide(I, created, D);
    else
        buildFunction(I, created, D);
}

template<std::derived_from<TypedefNameDecl> TypedefNameTy>
void
ASTVisitor::
traverse(
    TypedefNameTy* D,
    TypeAliasTemplateDecl* ATD)
{
    auto exp = getAsMrDocsInfo(D);
    if(! exp) { return; }
    auto [I, created] = *exp;

    if(isa<TypeAliasDecl>(D))
        I.IsUsing = true;

    if(ATD)
    {
        I.Template = std::make_unique<TemplateInfo>();
        buildTemplateParams(*I.Template,
            ATD->getTemplateParameters());
    }

    buildTypedef(I, created, D);
}

template<typename... Args>
void
ASTVisitor::
traverse(Decl* D, Args&&...)
{
    // If this is a DeclContext, traverse its members
    if(auto* DC = dyn_cast<DeclContext>(D))
        traverseContext(DC);
}

//------------------------------------------------

template<typename... Args>
void
ASTVisitor::
traverseDecl(
    Decl* D,
    Args&&... args)
{
    MRDOCS_ASSERT(D);

    // Decl had a semantic error or is implicitly
    // generated by the implementation
    if(D->isInvalidDecl() || D->isImplicit())
        return;

    SymbolFilter::FilterScope scope(symbolFilter_);

    // Convert to the most derived type of the Decl
    // and call the appropriate traverse function
    visit(D, [&]<typename DeclTy>(DeclTy* DD)
    {
        if constexpr(std::derived_from<DeclTy,
            RedeclarableTemplateDecl>)
        {
            // Only ClassTemplateDecl, FunctionTemplateDecl,
            // VarTemplateDecl, and TypeAliasDecl are derived
            // from RedeclarableTemplateDecl.
            // This doesn't include ConceptDecl.
            // Recursively call traverseDecl so traverse is called with
            // a pointer to the most derived type of the templated Decl
            traverseDecl(DD->getTemplatedDecl(), DD);
        }
        else if constexpr(std::derived_from<DeclTy,
            ClassTemplateSpecializationDecl>)
        {
            // A class template specialization
            traverse(DD, DD->getSpecializedTemplate());
        }
        else if constexpr(std::derived_from<DeclTy,
            VarTemplateSpecializationDecl>)
        {
            // A variable template specialization
            traverse(DD, DD->getSpecializedTemplate());
        }
        else
        {
            // Call the appropriate traverse function
            // for the most derived type of the Decl
            traverse(DD, std::forward<Args>(args)...);
        }
    });
}

void
ASTVisitor::
traverseContext(DeclContext* DC)
{
    for(auto* D : DC->decls())
        traverseDecl(D);
}

//------------------------------------------------

/** Get the user-written `Decl` for a `Decl`

    Given a `Decl` `D`, `InstantiatedFromVisitor` will return the
    user-written `Decl` corresponding to `D`. For specializations
    which were implicitly instantiated, this will be whichever `Decl`
    was used as the pattern for instantiation.
*/
class InstantiatedFromVisitor
    : public DeclVisitor<InstantiatedFromVisitor, Decl*>
{
public:
    Decl*
    VisitDecl(Decl* D)
    {
        return D;
    }

    FunctionDecl*
    VisitFunctionTemplateDecl(FunctionTemplateDecl* D)
    {
        while(auto* MT = D->getInstantiatedFromMemberTemplate())
        {
            if(D->isMemberSpecialization())
                break;
            D = MT;
        }
        return D->getTemplatedDecl();
    }

    CXXRecordDecl*
    VisitClassTemplateDecl(ClassTemplateDecl* D)
    {
        while (auto* MT = D->getInstantiatedFromMemberTemplate())
        {
            if(D->isMemberSpecialization())
                break;
            D = MT;
        }
        return D->getTemplatedDecl();
    }

    VarDecl*
    VisitVarTemplateDecl(VarTemplateDecl* D)
    {
        while(auto* MT = D->getInstantiatedFromMemberTemplate())
        {
            if(D->isMemberSpecialization())
                break;
            D = MT;
        }
        return D->getTemplatedDecl();
    }

    TypedefNameDecl*
    VisitTypeAliasTemplateDecl(TypeAliasTemplateDecl* D)
    {
        if(auto* MT = D->getInstantiatedFromMemberTemplate())
        {
            // KRYSTIAN NOTE: we don't really need to check this
            if(! D->isMemberSpecialization())
                D = MT;
        }
        return VisitTypedefNameDecl(D->getTemplatedDecl());
    }

    FunctionDecl*
    VisitFunctionDecl(FunctionDecl* D)
    {
        FunctionDecl const* DD = nullptr;
        if(D->isDefined(DD, false))
            D = const_cast<FunctionDecl*>(DD);

        if(MemberSpecializationInfo* MSI = D->getMemberSpecializationInfo())
        {
            if(! MSI->isExplicitSpecialization())
                D = cast<FunctionDecl>(
                    MSI->getInstantiatedFrom());
        }
        else if(D->getTemplateSpecializationKind() !=
            TSK_ExplicitSpecialization)
        {
            D = D->getFirstDecl();
            if(auto* FTD = D->getPrimaryTemplate())
                D = VisitFunctionTemplateDecl(FTD);
        }
        return D;
    }

    CXXRecordDecl*
    VisitClassTemplatePartialSpecializationDecl(ClassTemplatePartialSpecializationDecl* D)
    {
        while (auto* MT = D->getInstantiatedFromMember())
        {
            if(D->isMemberSpecialization())
                break;
            D = MT;
        }
        return VisitClassTemplateSpecializationDecl(D);
    }

    CXXRecordDecl*
    VisitClassTemplateSpecializationDecl(ClassTemplateSpecializationDecl* D)
    {
        if(! D->isExplicitSpecialization())
        {
            auto inst_from = D->getSpecializedTemplateOrPartial();
            if(auto* CTPSD = inst_from.dyn_cast<
                ClassTemplatePartialSpecializationDecl*>())
            {
                MRDOCS_ASSERT(D != CTPSD);
                return VisitClassTemplatePartialSpecializationDecl(CTPSD);
            }
            // Explicit instantiation declaration/definition
            else if(auto* CTD = inst_from.dyn_cast<
                ClassTemplateDecl*>())
            {
                return VisitClassTemplateDecl(CTD);
            }
        }
        return VisitCXXRecordDecl(D);
    }

    CXXRecordDecl*
    VisitCXXRecordDecl(CXXRecordDecl* D)
    {
        while(MemberSpecializationInfo* MSI =
            D->getMemberSpecializationInfo())
        {
            // if this is a member of an explicit specialization,
            // then we have the correct declaration
            if(MSI->isExplicitSpecialization())
                break;
            D = cast<CXXRecordDecl>(MSI->getInstantiatedFrom());
        }
        return D;
    }

    VarDecl*
    VisitVarTemplatePartialSpecializationDecl(VarTemplatePartialSpecializationDecl* D)
    {
        while(auto* MT = D->getInstantiatedFromMember())
        {
            if(D->isMemberSpecialization())
                break;
            D = MT;
        }
        return VisitVarTemplateSpecializationDecl(D);
    }

    VarDecl*
    VisitVarTemplateSpecializationDecl(VarTemplateSpecializationDecl* D)
    {
        if(! D->isExplicitSpecialization())
        {
            auto inst_from = D->getSpecializedTemplateOrPartial();
            if(auto* VTPSD = inst_from.dyn_cast<
                VarTemplatePartialSpecializationDecl*>())
            {
                MRDOCS_ASSERT(D != VTPSD);
                return VisitVarTemplatePartialSpecializationDecl(VTPSD);
            }
            // explicit instantiation declaration/definition
            else if(auto* VTD = inst_from.dyn_cast<
                VarTemplateDecl*>())
            {
                return VisitVarTemplateDecl(VTD);
            }
        }
        return VisitVarDecl(D);
    }

    VarDecl*
    VisitVarDecl(VarDecl* D)
    {
        while(MemberSpecializationInfo* MSI =
            D->getMemberSpecializationInfo())
        {
            if(MSI->isExplicitSpecialization())
                break;
            D = cast<VarDecl>(MSI->getInstantiatedFrom());
        }
        return D;
    }

    EnumDecl*
    VisitEnumDecl(EnumDecl* D)
    {
        while(MemberSpecializationInfo* MSI =
            D->getMemberSpecializationInfo())
        {
            if(MSI->isExplicitSpecialization())
                break;
            D = cast<EnumDecl>(MSI->getInstantiatedFrom());
        }
        return D;
    }

    TypedefNameDecl*
    VisitTypedefNameDecl(TypedefNameDecl* D)
    {
        DeclContext* Context = D->getNonTransparentDeclContext();
        if(Context->isFileContext())
            return D;
        DeclContext* ContextPattern =
            cast<DeclContext>(Visit(cast<Decl>(Context)));
        if(Context == ContextPattern)
            return D;
        auto lookup = ContextPattern->lookup(D->getDeclName());
        for(NamedDecl* ND : lookup)
        {
            if(auto* TND = dyn_cast<TypedefNameDecl>(ND))
                return TND;
            if(auto* TATD = dyn_cast<TypeAliasTemplateDecl>(ND))
                return TATD->getTemplatedDecl();
        }
        return D;
    }
};

template<typename DeclTy>
DeclTy*
ASTVisitor::
getInstantiatedFrom(DeclTy* D)
{
    if(! D)
        return nullptr;
    Decl* decayedD = const_cast<Decl*>(static_cast<const Decl*>(D));
    Decl* resultDecl = InstantiatedFromVisitor().Visit(decayedD);
    return cast<DeclTy>(resultDecl);
}

//------------------------------------------------

template<typename Derived>
class TerminalTypeVisitor
    : public TypeVisitor<TerminalTypeVisitor<Derived>, bool>
{
    friend class TerminalTypeVisitor::TypeVisitor;

    ASTVisitor& Visitor_;

    unsigned Quals_ = 0;
    bool IsPack_ = false;
    const NestedNameSpecifier* NNS_ = nullptr;

    Derived& getDerived()
    {
        return static_cast<Derived&>(*this);
    }

    bool
    VisitParenType(
        const ParenType* T)
    {
        return Visit(T->getInnerType());
    }

    bool
    VisitMacroQualified(
        const MacroQualifiedType* T)
    {
        return Visit(T->getUnderlyingType());
    }

    bool
    VisitAttributedType(
        const AttributedType* T)
    {
        return Visit(T->getModifiedType());
    }

    bool
    VisitAdjustedType(
        const AdjustedType* T)
    {
        return Visit(T->getOriginalType());
    }

    bool
    VisitUsingType(
        const UsingType* T)
    {
        return Visit(T->getUnderlyingType());
    }

    bool
    VisitSubstTemplateTypeParmType(
        const SubstTemplateTypeParmType* T)
    {
        return Visit(T->getReplacementType());
    }

    // ----------------------------------------------------------------

    bool
    VisitElaboratedType(
        const ElaboratedType* T)
    {
        NNS_ = T->getQualifier();
        return Visit(T->getNamedType());
    }

    bool
    VisitPackExpansionType(
        const PackExpansionType* T)
    {
        IsPack_ = true;
        return Visit(T->getPattern());
    }

    // ----------------------------------------------------------------

    bool
    VisitPointerType(
        const PointerType* T)
    {
        getDerived().buildPointer(T, std::exchange(Quals_, 0));
        return Visit(T->getPointeeType());
    }

    bool
    VisitLValueReferenceType(
        const LValueReferenceType* T)
    {
        getDerived().buildLValueReference(T);
        Quals_ = 0;
        return Visit(T->getPointeeType());
    }

    bool
    VisitRValueReferenceType(
        const RValueReferenceType* T)
    {
        getDerived().buildRValueReference(T);
        Quals_ = 0;
        return Visit(T->getPointeeType());
    }

    bool
    VisitMemberPointerType(
        const MemberPointerType* T)
    {
        getDerived().buildMemberPointer(T, std::exchange(Quals_, 0));
        return Visit(T->getPointeeType());
    }

    bool
    VisitFunctionType(
        const FunctionType* T)
    {
        getDerived().buildFunction(T);
        return Visit(T->getReturnType());
    }

    bool
    VisitArrayType(
        const ArrayType* T)
    {
        getDerived().buildArray(T);
        return Visit(T->getElementType());
    }

    // ----------------------------------------------------------------

    bool
    VisitDecltypeType(
        const DecltypeType* T)
    {
        getDerived().buildDecltype(T, Quals_, IsPack_);
        return true;
    }

    bool
    VisitAutoType(
        const AutoType* T)
    {
        // KRYSTIAN TODO: we should probably add a TypeInfo
        // to represent deduced types that also stores what
        // it was deduced as.
        // KRYSTIAN NOTE: we don't use isDeduced because it will
        // return true if the type is dependent
        // if the type has been deduced, use the deduced type
        if(QualType DT = T->getDeducedType(); ! DT.isNull())
            return Visit(DT);
        getDerived().buildTerminal(NNS_, T, Quals_, IsPack_);
        return true;
    }

    bool
    VisitDeducedTemplateSpecializationType(
        const DeducedTemplateSpecializationType* T)
    {
        // KRYSTIAN TODO: we should probably add a TypeInfo
        // to represent deduced types that also stores what
        // it was deduced as.
        if(QualType DT = T->getDeducedType(); ! DT.isNull())
            return Visit(DT);
        TemplateName TN = T->getTemplateName();
        MRDOCS_ASSERT(! TN.isNull());
        NamedDecl* ND = TN.getAsTemplateDecl();
        getDerived().buildTerminal(NNS_, ND,
            std::nullopt, Quals_, IsPack_);
        return true;
    }

    bool
    VisitDependentNameType(
        const DependentNameType* T)
    {
        if(auto* NNS = T->getQualifier())
            NNS_ = NNS;
        getDerived().buildTerminal(NNS_, T->getIdentifier(),
            std::nullopt, Quals_, IsPack_);
        return true;
    }

    bool
    VisitDependentTemplateSpecializationType(
        const DependentTemplateSpecializationType* T)
    {
        if(auto* NNS = T->getQualifier())
            NNS_ = NNS;
        getDerived().buildTerminal(NNS_, T->getIdentifier(),
            T->template_arguments(), Quals_, IsPack_);
        return true;
    }

    bool
    VisitTemplateSpecializationType(
        const TemplateSpecializationType* T)
    {
        TemplateName TN = T->getTemplateName();
        MRDOCS_ASSERT(! TN.isNull());
        NamedDecl* ND = TN.getAsTemplateDecl();
        if(! T->isTypeAlias())
        {
            auto* CT = T->getCanonicalTypeInternal().getTypePtrOrNull();
            if(auto* ICT = dyn_cast_or_null<InjectedClassNameType>(CT))
                ND = ICT->getDecl();
            else if(auto* RT = dyn_cast_or_null<RecordType>(CT))
                ND = RT->getDecl();
        }
        getDerived().buildTerminal(NNS_, ND,
            T->template_arguments(), Quals_, IsPack_);
        return true;
    }

    bool
    VisitRecordType(
        const RecordType* T)
    {
        RecordDecl* RD = T->getDecl();
        // if this is an instantiation of a class template,
        // create a SpecializationTypeInfo & extract the template arguments
        std::optional<ArrayRef<TemplateArgument>> TArgs = std::nullopt;
        if(auto* CTSD = dyn_cast<ClassTemplateSpecializationDecl>(RD))
            TArgs = CTSD->getTemplateArgs().asArray();
        getDerived().buildTerminal(NNS_, RD,
            TArgs, Quals_, IsPack_);
        return true;
    }

    bool
    VisitInjectedClassNameType(
        const InjectedClassNameType* T)
    {
        getDerived().buildTerminal(NNS_, T->getDecl(),
            std::nullopt, Quals_, IsPack_);
        return true;
    }

    bool
    VisitEnumType(
        const EnumType* T)
    {
        getDerived().buildTerminal(NNS_, T->getDecl(),
            std::nullopt, Quals_, IsPack_);
        return true;
    }

    bool
    VisitTypedefType(
        const TypedefType* T)
    {
        getDerived().buildTerminal(NNS_, T->getDecl(),
            std::nullopt, Quals_, IsPack_);
        return true;
    }

    bool
    VisitTemplateTypeParmType(
        const TemplateTypeParmType* T)
    {
        const IdentifierInfo* II = nullptr;
        if(TemplateTypeParmDecl* D = T->getDecl())
        {
            if(D->isImplicit())
            {
                // special case for implicit template parameters
                // resulting from abbreviated function templates
                getDerived().buildTerminal(
                    NNS_, T, Quals_, IsPack_);
                return true;
            }
            II = D->getIdentifier();
        }
        getDerived().buildTerminal(NNS_, II,
            std::nullopt, Quals_, IsPack_);
        return true;
    }

    bool
    VisitSubstTemplateTypeParmPackType(
        const SubstTemplateTypeParmPackType* T)
    {
        getDerived().buildTerminal(NNS_, T->getIdentifier(),
            std::nullopt, Quals_, IsPack_);
        return true;
    }

    bool
    VisitType(const Type* T)
    {
        getDerived().buildTerminal(
            NNS_, T, Quals_, IsPack_);
        return true;
    }

public:
    TerminalTypeVisitor(
        ASTVisitor& Visitor)
        : Visitor_(Visitor)
    {
    }

    ASTVisitor& getASTVisitor()
    {
        return Visitor_;
    }

    using TerminalTypeVisitor::TypeVisitor::Visit;

    bool
    Visit(QualType QT)
    {
        Quals_ |= QT.getLocalFastQualifiers();
        return Visit(QT.getTypePtrOrNull());
    }

    void
    buildPointer
        (const PointerType* T,
        unsigned quals)
    {
    }

    void
    buildLValueReference(
        const LValueReferenceType* T)
    {
    }

    void
    buildRValueReference(
        const RValueReferenceType* T)
    {
    }

    void
    buildMemberPointer(
        const MemberPointerType* T, unsigned quals)
    {
    }

    void
    buildArray(
        const ArrayType* T)
    {
    }

    void
    buildFunction(
        const FunctionType* T)
    {
    }

    void
    buildDecltype(
        const DecltypeType* T,
        unsigned quals,
        bool pack)
    {
    }

    void
    buildTerminal(
        const NestedNameSpecifier* NNS,
        const Type* T,
        unsigned quals,
        bool pack)
    {
    }

    void
    buildTerminal(
        const NestedNameSpecifier* NNS,
        const IdentifierInfo* II,
        std::optional<ArrayRef<TemplateArgument>> TArgs,
        unsigned quals,
        bool pack)
    {
    }

    void
    buildTerminal(
        const NestedNameSpecifier* NNS,
        const NamedDecl* D,
        std::optional<ArrayRef<TemplateArgument>> TArgs,
        unsigned quals,
        bool pack)
    {
    }
};

class TypeInfoBuilder
    : public TerminalTypeVisitor<TypeInfoBuilder>
{
    std::unique_ptr<TypeInfo> Result;
    std::unique_ptr<TypeInfo>* Inner = &Result;

public:
    TypeInfoBuilder(ASTVisitor& Visitor)
        : TerminalTypeVisitor(Visitor)
    {
    }

    std::unique_ptr<TypeInfo> result()
    {
        return std::move(Result);
    }

    void buildPointer(const PointerType* T, unsigned quals)
    {
        auto I = std::make_unique<PointerTypeInfo>();
        I->CVQualifiers = convertToQualifierKind(quals);
        *std::exchange(Inner, &I->PointeeType) = std::move(I);
    }

    void buildLValueReference(const LValueReferenceType* T)
    {
        auto I = std::make_unique<LValueReferenceTypeInfo>();
        *std::exchange(Inner, &I->PointeeType) = std::move(I);
    }

    void buildRValueReference(const RValueReferenceType* T)
    {
        auto I = std::make_unique<RValueReferenceTypeInfo>();
        *std::exchange(Inner, &I->PointeeType) = std::move(I);
    }

    void buildMemberPointer(const MemberPointerType* T, unsigned quals)
    {
        auto I = std::make_unique<MemberPointerTypeInfo>();
        I->CVQualifiers = convertToQualifierKind(quals);
        // do not set NNS because the parent type is *not*
        // a nested-name-specifier which qualifies the pointee type
        I->ParentType = getASTVisitor().buildTypeInfo(
            QualType(T->getClass(), 0));
        *std::exchange(Inner, &I->PointeeType) = std::move(I);
    }

    void buildArray(const ArrayType* T)
    {
        auto I = std::make_unique<ArrayTypeInfo>();
        if(auto* CAT = dyn_cast<ConstantArrayType>(T))
            getASTVisitor().buildExprInfo(
                I->Bounds, CAT->getSizeExpr(), CAT->getSize());
        else if(auto* DAT = dyn_cast<DependentSizedArrayType>(T))
            getASTVisitor().buildExprInfo(
                I->Bounds, DAT->getSizeExpr());
        *std::exchange(Inner, &I->ElementType) = std::move(I);
    }

    void buildFunction(const FunctionType* T)
    {
        auto* FPT = cast<FunctionProtoType>(T);
        auto I = std::make_unique<FunctionTypeInfo>();
        for(QualType PT : FPT->getParamTypes())
            I->ParamTypes.emplace_back(
                getASTVisitor().buildTypeInfo(PT));
        I->RefQualifier = convertToReferenceKind(
            FPT->getRefQualifier());
        I->CVQualifiers = convertToQualifierKind(
            FPT->getMethodQuals().getFastQualifiers());
        I->IsVariadic = FPT->isVariadic();
        getASTVisitor().buildNoexceptInfo(I->ExceptionSpec, FPT);
        *std::exchange(Inner, &I->ReturnType) = std::move(I);
    }

    void
    buildDecltype(
        const DecltypeType* T,
        unsigned quals,
        bool pack)
    {
        auto I = std::make_unique<DecltypeTypeInfo>();
        getASTVisitor().buildExprInfo(
            I->Operand, T->getUnderlyingExpr());
        I->CVQualifiers = convertToQualifierKind(quals);
        *Inner = std::move(I);
        Result->IsPackExpansion = pack;
    }

    void
    buildTerminal(
        const NestedNameSpecifier* NNS,
        const Type* T,
        unsigned quals,
        bool pack)
    {
        auto I = std::make_unique<NamedTypeInfo>();
        I->CVQualifiers = convertToQualifierKind(quals);

        auto Name = std::make_unique<NameInfo>();
        Name->Name = getASTVisitor().getTypeAsString(T);
        Name->Prefix = getASTVisitor().buildNameInfo(NNS);
        I->Name = std::move(Name);
        *Inner = std::move(I);
        Result->IsPackExpansion = pack;
    }

    void
    buildTerminal(
        const NestedNameSpecifier* NNS,
        const IdentifierInfo* II,
        std::optional<ArrayRef<TemplateArgument>> TArgs,
        unsigned quals,
        bool pack)
    {
        ASTVisitor& V = getASTVisitor();
        auto I = std::make_unique<NamedTypeInfo>();
        I->CVQualifiers = convertToQualifierKind(quals);

        if(TArgs)
        {
            auto Name = std::make_unique<SpecializationNameInfo>();
            if(II)
                Name->Name = II->getName();
            Name->Prefix = getASTVisitor().buildNameInfo(NNS);
            V.buildTemplateArgs(Name->TemplateArgs, *TArgs);
            I->Name = std::move(Name);
        }
        else
        {
            auto Name = std::make_unique<NameInfo>();
            if(II)
                Name->Name = II->getName();
            Name->Prefix = getASTVisitor().buildNameInfo(NNS);
            I->Name = std::move(Name);
        }
        *Inner = std::move(I);
        Result->IsPackExpansion = pack;
    }

    void
    buildTerminal(
        const NestedNameSpecifier* NNS,
        const NamedDecl* D,
        std::optional<ArrayRef<TemplateArgument>> TArgs,
        unsigned quals,
        bool pack)
    {
        ASTVisitor& V = getASTVisitor();
        auto I = std::make_unique<NamedTypeInfo>();
        I->CVQualifiers = convertToQualifierKind(quals);

        if(TArgs)
        {
            auto Name = std::make_unique<SpecializationNameInfo>();
            if(const IdentifierInfo* II = D->getIdentifier())
                Name->Name = II->getName();
            V.getDependencyID(V.getInstantiatedFrom(D), Name->id);
            if(NNS)
                Name->Prefix = V.buildNameInfo(NNS);
            else
                Name->Prefix = V.buildNameInfo(V.getParentDecl(D));

            V.buildTemplateArgs(Name->TemplateArgs, *TArgs);
            I->Name = std::move(Name);
        }
        else
        {
            auto Name = std::make_unique<NameInfo>();
            if(const IdentifierInfo* II = D->getIdentifier())
                Name->Name = II->getName();
            V.getDependencyID(V.getInstantiatedFrom(D), Name->id);
            if(NNS)
                Name->Prefix = V.buildNameInfo(NNS);
            else
                Name->Prefix = V.buildNameInfo(V.getParentDecl(D));
            I->Name = std::move(Name);
        }
        *Inner = std::move(I);
        Result->IsPackExpansion = pack;
    }
};

std::unique_ptr<TypeInfo>
ASTVisitor::
buildTypeInfo(
    QualType qt,
    ExtractMode extract_mode)
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

class NameInfoBuilder
    : public TerminalTypeVisitor<NameInfoBuilder>
{
    std::unique_ptr<NameInfo> Result;

public:
    NameInfoBuilder(ASTVisitor& Visitor)
        : TerminalTypeVisitor(Visitor)
    {
    }

    std::unique_ptr<NameInfo> result()
    {
        return std::move(Result);
    }

    void
    buildDecltype(
        const DecltypeType* T,
        unsigned quals,
        bool pack)
    {
        // KRYSTIAN TODO: support decltype in names
        // (e.g. within nested-name-specifiers).
    }

    void
    buildTerminal(
        const NestedNameSpecifier* NNS,
        const Type* T,
        unsigned quals,
        bool pack)
    {
        auto I = std::make_unique<NameInfo>();
        I->Name = getASTVisitor().getTypeAsString(T);
        Result = std::move(I);
        if(NNS)
            Result->Prefix = getASTVisitor().buildNameInfo(NNS);
    }

    void
    buildTerminal(
        const NestedNameSpecifier* NNS,
        const IdentifierInfo* II,
        std::optional<ArrayRef<TemplateArgument>> TArgs,
        unsigned quals,
        bool pack)
    {
        ASTVisitor& V = getASTVisitor();
        if(TArgs)
        {
            auto I = std::make_unique<SpecializationNameInfo>();
            if(II)
                I->Name = II->getName();
            V.buildTemplateArgs(I->TemplateArgs, *TArgs);
            Result = std::move(I);
        }
        else
        {
            auto I = std::make_unique<NameInfo>();
            if(II)
                I->Name = II->getName();
            Result = std::move(I);
        }
        if(NNS)
            Result->Prefix = V.buildNameInfo(NNS);
    }

    void
    buildTerminal(
        const NestedNameSpecifier* NNS,
        const NamedDecl* D,
        std::optional<ArrayRef<TemplateArgument>> TArgs,
        unsigned quals,
        bool pack)
    {
        ASTVisitor& V = getASTVisitor();
        const IdentifierInfo* II = D->getIdentifier();
        if(TArgs)
        {
            auto I = std::make_unique<SpecializationNameInfo>();
            if(II)
                I->Name = II->getName();
            V.getDependencyID(V.getInstantiatedFrom(D), I->id);
            V.buildTemplateArgs(I->TemplateArgs, *TArgs);
            Result = std::move(I);
        }
        else
        {
            auto I = std::make_unique<NameInfo>();
            if(II)
                I->Name = II->getName();
            V.getDependencyID(V.getInstantiatedFrom(D), I->id);
            Result = std::move(I);
        }
        if(NNS)
            Result->Prefix = V.buildNameInfo(NNS);
        else
            Result->Prefix = V.buildNameInfo(V.getParentDecl(D));
    }
};

std::unique_ptr<NameInfo>
ASTVisitor::
buildNameInfo(
    const NestedNameSpecifier* NNS,
    ExtractMode extract_mode)
{
    ExtractionScope scope = enterMode(extract_mode);

    std::unique_ptr<NameInfo> I = nullptr;
    if(! NNS)
        return I;
    if(const Type* T = NNS->getAsType())
    {
        NameInfoBuilder Builder(*this);
        Builder.Visit(T);
        I = Builder.result();
    }
    else if(const IdentifierInfo* II = NNS->getAsIdentifier())
    {
        I = std::make_unique<NameInfo>();
        I->Name = II->getName();
        I->Prefix = buildNameInfo(NNS->getPrefix(), extract_mode);
    }
    else if(const NamespaceDecl* ND = NNS->getAsNamespace())
    {
        I = std::make_unique<NameInfo>();
        I->Name = ND->getIdentifier()->getName();
        getDependencyID(ND, I->id);
        I->Prefix = buildNameInfo(getParentDecl(ND), extract_mode);
    }
    else if(const NamespaceAliasDecl* NAD = NNS->getAsNamespaceAlias())
    {
        I = std::make_unique<NameInfo>();
        I->Name = NAD->getIdentifier()->getName();
        const NamespaceDecl* ND = NAD->getNamespace();
        // KRYSTIAN FIXME: this should use the SymbolID of the namespace alias
        // once we add an Info kind to represent them
        getDependencyID(ND, I->id);
        I->Prefix = buildNameInfo(getParentDecl(ND), extract_mode);
    }
    return I;
}

std::unique_ptr<NameInfo>
ASTVisitor::
buildNameInfo(
    const Decl* D,
    ExtractMode extract_mode)
{
    const auto* ND = dyn_cast_if_present<NamedDecl>(D);
    if(! ND || ND->getKind() == Decl::TranslationUnit)
        return nullptr;
    auto I = std::make_unique<NameInfo>();
    if(const IdentifierInfo* II = ND->getIdentifier())
        I->Name = II->getName();
    getDependencyID(getInstantiatedFrom(D), I->id);
    I->Prefix = buildNameInfo(getParentDecl(D), extract_mode);
    return I;
}

//------------------------------------------------

#if 0
class ASTInstantiationCallbacks
    : public TemplateInstantiationCallback
{
    void initialize(const Sema&) override { }
    void finalize(const Sema&) override { }

    void
    atTemplateBegin(
        const Sema& S,
        const Sema::CodeSynthesisContext& Context) override
    {
    }

    void
    atTemplateEnd(
        const Sema& S,
        const Sema::CodeSynthesisContext& Context) override
    {
        if(Context.Kind != Sema::CodeSynthesisContext::TemplateInstantiation)
            return;

        Decl* D = Context.Entity;
        if(! D)
            return;

        bool skip = visit(D, [&]<typename DeclTy>(DeclTy* DT) -> bool
        {
            constexpr Decl::Kind kind = DeclToKind<DeclTy>();

            if constexpr(std::derived_from<DeclTy, CXXMethodDecl>)
            {
                CXXMethodDecl* MD = DT;
                return MD->isFunctionTemplateSpecialization();
            }

            if constexpr(kind == Decl::TypeAliasTemplate)
            {
                TypeAliasTemplateDecl* TAD = DT;

                if(DeclContext* DC = TAD->getDeclContext())
                {
                    Decl* DCD = cast<Decl>(DC);
                    return ! DCD->isImplicit();
                }
            }

            return false;
        });

        if(skip)
            return;

        D->setImplicit();
    }
};
#endif


//------------------------------------------------
//
// ASTVisitorConsumer
//
//------------------------------------------------

class ASTVisitorConsumer
    : public SemaConsumer
{
    const ConfigImpl& config_;
    ExecutionContext& ex_;
    CompilerInstance& compiler_;

    Sema* sema_ = nullptr;

    void
    InitializeSema(Sema& S) override
    {
        // Sema should not have been initialized yet
        MRDOCS_ASSERT(! sema_);
        sema_ = &S;

#if 0
        S.TemplateInstCallbacks.emplace_back(
            std::make_unique<ASTInstantiationCallbacks>());
#endif
    }

    void
    ForgetSema() override
    {
        sema_ = nullptr;
    }

    void
    HandleTranslationUnit(ASTContext& Context) override
    {
        // the Sema better be valid
        MRDOCS_ASSERT(sema_);

        // initialize the diagnostics reporter first
        // so errors prior to traversal are reported
        Diagnostics diags;

        // loads and caches source files into memory
        SourceManager& source = Context.getSourceManager();
        // get the name of the translation unit.
        // will be std::nullopt_t if it isn't a file
        std::optional<llvm::SmallString<128>> file_name =
            source.getNonBuiltinFilenameForID(source.getMainFileID());
        // KRYSTIAN NOTE: should we report anything here?
        if (!file_name)
        {
            return;
        }

        // skip the translation unit if configured to do so
        convert_to_slash(*file_name);

        ASTVisitor visitor(
            config_,
            diags,
            compiler_,
            Context,
            *sema_);

        // Traverse the translation unit
        visitor.build();

        // VFALCO If we returned from the function early
        // then this line won't execute, which means we
        // will miss error and warnings emitted before
        // the return.
        ex_.report(std::move(visitor.results()), std::move(diags));
    }

    /** Skip function bodies

        This is called by Sema when parsing a function that has a body and:
        - is constexpr, or
        - uses a placeholder for a deduced return type

        We always return `true` because whenever this function *is* called,
        it will be for a function that cannot be used in a constant expression,
        nor one that introduces a new type via returning a local class.
    */
    bool
    shouldSkipFunctionBody(Decl*) override
    {
        return true;
    }

    bool
    HandleTopLevelDecl(DeclGroupRef) override
    {
        return true;
    }

    ASTMutationListener*
    GetASTMutationListener() override
    {
        return nullptr;
    }

    void
    HandleCXXStaticMemberVarInstantiation(VarDecl* D) override
    {
        // implicitly instantiated definitions of non-inline
        // static data members of class templates are added to
        // the end of the TU DeclContext. Decl::isImplicit returns
        // false for these VarDecls, so we manually set it here.
        D->setImplicit();
    }

    void
    HandleCXXImplicitFunctionInstantiation(FunctionDecl* D) override
    {
        D->setImplicit();
    }

    void HandleInlineFunctionDefinition(FunctionDecl*) override { }
    void HandleTagDeclDefinition(TagDecl*) override { }
    void HandleTagDeclRequiredDefinition(const TagDecl*) override { }
    void HandleInterestingDecl(DeclGroupRef) override { }
    void CompleteTentativeDefinition(VarDecl*) override { }
    void CompleteExternalDeclaration(VarDecl*) override { }
    void AssignInheritanceModel(CXXRecordDecl*) override { }
    void HandleVTable(CXXRecordDecl*) override { }
    void HandleImplicitImportDecl(ImportDecl*) override { }
    void HandleTopLevelDeclInObjCContainer(DeclGroupRef) override { }

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
};

//------------------------------------------------
//
// ASTAction
//
//------------------------------------------------

/** A frontend action for visiting the AST

    This is used by the tooling infrastructure to create
    an ASTAction for each translation unit.

    The ASTAction is responsible for creating the ASTConsumer
    which will be used to traverse the AST.
*/
struct ASTAction
    : public clang::ASTFrontendAction
{
    ASTAction(
        ExecutionContext& ex,
        ConfigImpl const& config) noexcept
        : ex_(ex)
        , config_(config)
    {
    }

    /** Execute the action

        This is called by the tooling infrastructure to execute
        the action for each translation unit.

        The action will parse the AST with the consumer
        that should have been previously created with
        CreateASTConsumer.

        This consumer then creates a ASTVisitor that
        will convert the AST into a set of MrDocs Info
        objects.
     */
    void
    ExecuteAction() override
    {
        CompilerInstance& CI = getCompilerInstance();
        if (!CI.hasPreprocessor())
        {
            return;
        }

        // Ensure comments in system headers are retained.
        // We may want them if, e.g., a declaration was extracted
        // as a dependency
        CI.getLangOpts().RetainCommentsFromSystemHeaders = true;

        if (!CI.hasSema())
        {
            CI.createSema(getTranslationUnitKind(), nullptr);
        }

        ParseAST(
            CI.getSema(),
            false, // ShowStats
            true); // SkipFunctionBodies
    }

    /** Create the object that will traverse the AST

        This is called by the tooling infrastructure to create
        an ASTConsumer for each translation unit.

        This consumer creates a ASTVisitor that will convert
        the AST into a set of our objects.

        The main function of the ASTVisitorConsumer is
        the HandleTranslationUnit function, which is called
        to traverse the AST.

     */
    std::unique_ptr<clang::ASTConsumer>
    CreateASTConsumer(
        clang::CompilerInstance& Compiler,
        llvm::StringRef InFile) override
    {
        return std::make_unique<ASTVisitorConsumer>(
            config_, ex_, Compiler);
    }

private:
    ExecutionContext& ex_;
    ConfigImpl const& config_;
};

//------------------------------------------------

/** A frontend action factory for ASTAction

    This is used by the tooling infrastructure to create
    an ASTAction for each translation unit.
*/
struct ASTActionFactory :
    tooling::FrontendActionFactory
{
    ASTActionFactory(
        ExecutionContext& ex,
        ConfigImpl const& config) noexcept
        : ex_(ex)
        , config_(config)
    {
    }

    std::unique_ptr<FrontendAction>
    create() override
    {
        return std::make_unique<ASTAction>(ex_, config_);
    }

private:
    ExecutionContext& ex_;
    ConfigImpl const& config_;
};

} // (anon)

//------------------------------------------------

std::unique_ptr<tooling::FrontendActionFactory>
makeFrontendActionFactory(
    ExecutionContext& ex,
    ConfigImpl const& config)
{
    return std::make_unique<ASTActionFactory>(ex, config);
}

} // mrdocs
} // clang
