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

#include "ASTVisitor.hpp"
#include "ASTVisitorHelpers.hpp"
#include "Bitcode.hpp"
#include "ParseJavadoc.hpp"
#include "lib/Support/Path.hpp"
#include "lib/Support/Debug.hpp"
#include "lib/Lib/Diagnostics.hpp"
#include "lib/Lib/Filters.hpp"
#include "lib/Lib/Info.hpp"
#include <mrdox/Metadata.hpp>
#include <clang/AST/AST.h>
#include <clang/AST/Attr.h>
#include <clang/AST/DeclFriend.h>
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
namespace mrdox {

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

    struct FileFilter
    {
        std::string prefix;
        bool include = true;
    };

    std::unordered_map<
        clang::SourceLocation::UIntTy,
        FileFilter> fileFilter_;

    llvm::SmallString<512> file_;
    bool isFileInRootDir_ = false;

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
        MRDOX_ASSERT(context_.getTraversalScope() ==
            std::vector<Decl*>{context_.getTranslationUnitDecl()});
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
            SymbolID id = extractSymbolID(D);
            // skip declarations which generate invalid symbol IDs,
            // or which already have been extract
            if(id == SymbolID::zero || info_.contains(id))
                continue;
            traverseDecl(D);
        }

        // perform another pass if there are new dependencies
        if(! dependencies_.empty())
            buildDependencies(previous);
    }

    //------------------------------------------------

    Info*
    getInfo(const SymbolID& id)
    {
        if(auto it = info_.find(id); it != info_.end())
            return it->get();
        return nullptr;
    }

    template<typename InfoTy>
    std::pair<InfoTy&, bool>
    getOrCreateInfo(const SymbolID& id)
    {
        Info* info = getInfo(id);
        bool created = false;
        if(! info)
        {
            auto [it, inserted] = info_.emplace(
                std::make_unique<InfoTy>(id));
            info = it->get();
            created = true;
        }
        MRDOX_ASSERT(info->Kind == InfoTy::kind_id);
        info->Implicit &= currentMode() != ExtractMode::Normal;
        return {static_cast<InfoTy&>(*info), created};
    }

    void
    getDependencyID(
        Decl* D,
        SymbolID& id)
    {
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

        // add the adjusted declaration to the set of dependencies
        dependencies_.insert(Outer);
    }

    //------------------------------------------------

    // Function to hash a given USR value for storage.
    // As USRs (Unified Symbol Resolution) could bef
    // large, especially for functions with long type
    // arguments, we use 160-bits SHA1(USR) values to
    // guarantee the uniqueness of symbols while using
    // a relatively small amount of memory (vs storing
    // USRs directly).
    //
    bool
    extractSymbolID(
        const Decl* D,
        SymbolID& id)
    {
        // functions require their parameter types to be decayed
        // prior to USR generator to ensure that declarations
        // with parameter types which decay to the same type
        // generate the same USR
        if(const auto* FD = dyn_cast<FunctionDecl>(D))
            applyDecayToParameters(FD);
        usr_.clear();
        if(index::generateUSRForDecl(D, usr_))
            return false;
        id = SymbolID(llvm::SHA1::hash(
            arrayRefFromStringRef(usr_)).data());
        return true;
    }

    SymbolID
    extractSymbolID(
        const Decl* D)
    {
        SymbolID id = SymbolID::zero;
        extractSymbolID(D, id);
        return id;
    }

    bool
    shouldSerializeInfo(
        const NamedDecl* D) noexcept
    {
        // KRYSTIAN FIXME: getting the access of a members
        // is not as simple as calling Decl::getAccessUnsafe.
        // specifically, templates may not have
        // their access set until they are actually instantiated.
        return true;
    #if 0
        if(config_.includePrivate)
            return true;
        if(IsOrIsInAnonymousNamespace)
            return false;
        // bool isPublic()
        AccessSpecifier access = D->getAccessUnsafe();
        if(access == AccessSpecifier::AS_private)
            return false;
        Linkage linkage = D->getLinkageInternal();
        if(linkage == Linkage::ModuleLinkage ||
            linkage == Linkage::ExternalLinkage)
            return true;
        // some form of internal linkage
        return false;
    #endif
    }

    //------------------------------------------------

    unsigned
    getLine(
        const NamedDecl* D) const
    {
        return source_.getPresumedLoc(
            D->getBeginLoc()).getLine();
    }

    void
    addSourceLocation(
        SourceInfo& I,
        unsigned line,
        bool definition)
    {
        if(definition)
        {
            if(I.DefLoc)
                return;
            I.DefLoc.emplace(line, file_.str(), isFileInRootDir_);
        }
        else
        {
            auto existing = std::find_if(I.Loc.begin(), I.Loc.end(),
                [this, line](const Location& l)
                {
                    return l.LineNumber == line &&
                        l.Filename == file_.str();
                });
            if(existing != I.Loc.end())
                return;
            I.Loc.emplace_back(line, file_.str(), isFileInRootDir_);
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

    std::string
    getTypeAsString(
        QualType T)
    {
        return T.getAsString(context_.getPrintingPolicy());
    }

    NamedDecl*
    lookupTypedefInPrimary(
        TypedefNameDecl* TD)
    {
        if(auto* R = dyn_cast<CXXRecordDecl>(
            TD->getDeclContext()))
        {
            if(CXXRecordDecl* IP = R->getTemplateInstantiationPattern())
                R = IP;
            if(DeclarationName TDN = TD->getDeclName();
                R && ! TDN.isEmpty())
            {
                auto found = R->lookup(TDN);
                MRDOX_ASSERT(found.isSingleResult());
                MRDOX_ASSERT(isa<TypedefNameDecl>(found.front()) ||
                    isa<TypeAliasTemplateDecl>(found.front()));
                return found.front();
            }
        }
        return nullptr;
    }

    template<typename TypeInfoTy>
    std::unique_ptr<TypeInfoTy>
    makeTypeInfo(
        const IdentifierInfo* II,
        unsigned quals)
    {
        auto I = std::make_unique<TypeInfoTy>();
        I->CVQualifiers = convertToQualifierKind(quals);
        if(II)
            I->Name = II->getName();
        return I;
    }

    template<typename TypeInfoTy>
    std::unique_ptr<TypeInfoTy>
    makeTypeInfo(
        NamedDecl* N,
        unsigned quals)
    {
        auto I = makeTypeInfo<TypeInfoTy>(
            N->getIdentifier(), quals);

        N = cast<NamedDecl>(getInstantiatedFrom(N));
        // do not generate references to implicit declarations,
        // template template parameters, or builtin templates
        if(! isa<TemplateTemplateParmDecl>(N) &&
            ! isa<BuiltinTemplateDecl>(N))
        {
            if(auto* TD = dyn_cast<TypedefNameDecl>(N))
            {
                if(auto* PTD = lookupTypedefInPrimary(TD))
                    N = PTD;
            }
            else if(auto* ATD = dyn_cast<TypeAliasTemplateDecl>(N))
            {
                if(auto* MT = ATD->getInstantiatedFromMemberTemplate())
                    ATD = MT;
                auto* TD = ATD->getTemplatedDecl();
                if(auto* R = dyn_cast<CXXRecordDecl>(TD->getDeclContext()))
                {
                    // KRYSTIAN FIXME: this appears to not work
                    if(auto* PATD = lookupTypedefInPrimary(TD))
                        N = PATD;
                }
            }

            getDependencyID(N, I->id);
        }
        return I;
    }

    template<
        std::same_as<SpecializationTypeInfo> TypeInfoTy,
        typename DeclOrName,
        typename TArgsRange>
    std::unique_ptr<TypeInfoTy>
    makeTypeInfo(
        DeclOrName&& N,
        unsigned quals,
        TArgsRange&& targs)
    {
        auto I = makeTypeInfo<TypeInfoTy>(
            std::forward<DeclOrName>(N), quals);
        buildTemplateArgs(I->TemplateArgs,
            std::forward<TArgsRange>(targs));
        return I;
    }

    void
    buildParentTypeInfo(
        std::unique_ptr<TypeInfo>& Parent,
        const NestedNameSpecifier* NNS,
        ExtractMode extract_mode)
    {
        if(! NNS)
            return;
        // extraction for parents of a terminal TypeInfo
        // node use the same mode as that node
        if(const auto* T = NNS->getAsType())
        {
            Parent = buildTypeInfo(
                QualType(T, 0),
                extract_mode);
        }
        else if(const auto* II = NNS->getAsIdentifier())
        {
            auto R = std::make_unique<TagTypeInfo>();
            buildParentTypeInfo(
                R->ParentType,
                NNS->getPrefix(),
                extract_mode);
            R->Name = II->getName();
            Parent = std::move(R);
        }
    }

    std::unique_ptr<TypeInfo>
    buildTypeInfo(
        QualType qt,
        ExtractMode extract_mode = ExtractMode::IndirectDependency)
    {
        // extract_mode is only used during the extraction
        // the terminal type & its parents; the extraction of
        // function parameters, template arguments, and the parent class
        // of member pointers is done in ExtractMode::IndirectDependency
        ExtractionScope scope = enterMode(extract_mode);

        std::unique_ptr<TypeInfo> result;
        std::unique_ptr<TypeInfo>* inner = &result;

        // nested name specifier used for the terminal type node
        NestedNameSpecifier* NNS = nullptr;

        // whether this is a pack expansion
        bool is_pack_expansion = false;

        while(true)
        {
            // should never be called for a null QualType
            MRDOX_ASSERT(! qt.isNull());
            const Type* type = qt.getTypePtr();
            unsigned quals = qt.getLocalFastQualifiers();

            switch(qt->getTypeClass())
            {
            // parenthesized types
            case Type::Paren:
            {
                auto* T = cast<ParenType>(type);
                qt = T->getInnerType().withFastQualifiers(quals);
                continue;
            }
            case Type::MacroQualified:
            {
                auto* T = cast<MacroQualifiedType>(type);
                qt = T->getUnderlyingType().withFastQualifiers(quals);
                continue;
            }
            // type with __atribute__
            case Type::Attributed:
            {
                auto* T = cast<AttributedType>(type);
                qt = T->getModifiedType().withFastQualifiers(quals);
                continue;
            }
            // adjusted and decayed types
            case Type::Decayed:
            case Type::Adjusted:
            {
                auto* T = cast<AdjustedType>(type);
                qt = T->getOriginalType().withFastQualifiers(quals);
                continue;
            }
            // using declarations
            case Type::Using:
            {
                auto* T = cast<UsingType>(type);
                // look through the using declaration and
                // use the the type from the referenced declaration
                qt = T->getUnderlyingType().withFastQualifiers(quals);
                continue;
            }
            case Type::SubstTemplateTypeParm:
            {
                auto* T = cast<SubstTemplateTypeParmType>(type);
                qt = T->getReplacementType().withFastQualifiers(quals);
                continue;
            }
            // pack expansion
            case Type::PackExpansion:
            {
                auto* T = cast<PackExpansionType>(type);
                // we just use a flag to represent whether this is
                // a pack expansion rather than a type kind
                is_pack_expansion = true;
                qt = T->getPattern().withFastQualifiers(quals);
                continue;
            }
            // pointers
            case Type::Pointer:
            {
                auto* T = cast<PointerType>(type);
                auto I = std::make_unique<PointerTypeInfo>();
                I->CVQualifiers = convertToQualifierKind(quals);
                *std::exchange(inner, &I->PointeeType) = std::move(I);
                qt = T->getPointeeType();
                continue;
            }
            // references
            case Type::LValueReference:
            {
                auto* T = cast<LValueReferenceType>(type);
                auto I = std::make_unique<LValueReferenceTypeInfo>();
                *std::exchange(inner, &I->PointeeType) = std::move(I);
                qt = T->getPointeeType();
                continue;
            }
            case Type::RValueReference:
            {
                auto* T = cast<RValueReferenceType>(type);
                auto I = std::make_unique<RValueReferenceTypeInfo>();
                *std::exchange(inner, &I->PointeeType) = std::move(I);
                qt = T->getPointeeType();
                continue;
            }
            // pointer to members
            case Type::MemberPointer:
            {
                auto* T = cast<MemberPointerType>(type);
                auto I = std::make_unique<MemberPointerTypeInfo>();
                I->CVQualifiers = convertToQualifierKind(quals);
                // do not set NNS because the parent type is *not*
                // a nested-name-specifier which qualifies the pointee type
                I->ParentType = buildTypeInfo(QualType(T->getClass(), 0));
                *std::exchange(inner, &I->PointeeType) = std::move(I);
                qt = T->getPointeeType();
                continue;
            }
            // KRYSTIAN NOTE: we don't handle FunctionNoProto here,
            // and it's unclear if we should. we should not encounter
            // such types in c++ (but it might be possible?)
            // functions
            case Type::FunctionProto:
            {
                auto* T = cast<FunctionProtoType>(type);
                auto I = std::make_unique<FunctionTypeInfo>();
                for(QualType PT : T->getParamTypes())
                    I->ParamTypes.emplace_back(buildTypeInfo(PT));
                I->RefQualifier = convertToReferenceKind(
                    T->getRefQualifier());
                I->CVQualifiers = convertToQualifierKind(
                    T->getMethodQuals().getFastQualifiers());
                I->ExceptionSpec = convertToNoexceptKind(
                    T->getExceptionSpecType());
                *std::exchange(inner, &I->ReturnType) = std::move(I);
                qt = T->getReturnType();
                continue;
            }
            // KRYSTIAN FIXME: do we handle variables arrays?
            // they can only be created within function scope
            // arrays
            case Type::IncompleteArray:
            {
                auto* T = cast<IncompleteArrayType>(type);
                auto I = std::make_unique<ArrayTypeInfo>();
                *std::exchange(inner, &I->ElementType) = std::move(I);
                qt = T->getElementType();
                continue;
            }
            case Type::ConstantArray:
            {
                auto* T = cast<ConstantArrayType>(type);
                auto I = std::make_unique<ArrayTypeInfo>();
                // KRYSTIAN FIXME: this is broken; cannonical
                // constant array types never have a size expression
                buildExprInfo(I->Bounds, T->getSizeExpr(), T->getSize());
                *std::exchange(inner, &I->ElementType) = std::move(I);
                qt = T->getElementType();
                continue;
            }
            case Type::DependentSizedArray:
            {
                auto* T = cast<DependentSizedArrayType>(type);
                auto I = std::make_unique<ArrayTypeInfo>();
                buildExprInfo(I->Bounds, T->getSizeExpr());
                *std::exchange(inner, &I->ElementType) = std::move(I);
                qt = T->getElementType();
                continue;
            }

            // ------------------------------------------------
            // terminal TypeInfo nodes

            case Type::Auto:
            {
                auto* T = cast<AutoType>(type);
                QualType deduced = T->getDeducedType();
                // KRYSTIAN NOTE: we don't use isDeduced because it will
                // return true if the type is dependent
                // if the type has been deduced, use the deduced type
                if(! deduced.isNull())
                {
                    qt = deduced;
                    continue;
                }
                // otherwise, use the placeholder type specifier
                auto I = std::make_unique<BuiltinTypeInfo>();
                I->Name = getTypeAsString(
                    qt.withoutLocalFastQualifiers());
                I->CVQualifiers = convertToQualifierKind(quals);
                *inner = std::move(I);
                break;
            }
            case Type::DeducedTemplateSpecialization:
            {
                auto* T = cast<DeducedTemplateSpecializationType>(type);
                QualType deduced = T->getDeducedType();
                // if(! T->isDeduced())
                if(! deduced.isNull())
                {
                    qt = deduced;
                    continue;
                }
                *inner = makeTypeInfo<TagTypeInfo>(
                    T->getTemplateName().getAsTemplateDecl(), quals);
                break;
            }
            // elaborated type specifier or
            // type with nested name specifier
            case Type::Elaborated:
            {
                auto* T = cast<ElaboratedType>(type);
                // there should only ever be one
                // nested-name-specifier for the terminal type
                MRDOX_ASSERT(! NNS || ! T->getQualifier());
                NNS = T->getQualifier();
                qt = T->getNamedType().withFastQualifiers(quals);
                continue;
            }
            // qualified dependent name with template keyword
            case Type::DependentTemplateSpecialization:
            {
                auto* T = cast<DependentTemplateSpecializationType>(type);
                auto I = makeTypeInfo<SpecializationTypeInfo>(
                    T->getIdentifier(), quals, T->template_arguments());
                // there should only ever be one
                // nested-name-specifier for the terminal type
                MRDOX_ASSERT(! NNS || ! T->getQualifier());
                NNS = T->getQualifier();
                *inner = std::move(I);
                break;
            }
            // dependent typename-specifier
            case Type::DependentName:
            {
                auto* T = cast<DependentNameType>(type);
                auto I = makeTypeInfo<TagTypeInfo>(
                    T->getIdentifier(), quals);
                // there should only ever be one
                // nested-name-specifier for the terminal type
                MRDOX_ASSERT(! NNS || ! T->getQualifier());
                NNS = T->getQualifier();
                *inner = std::move(I);
                break;
            }
            // specialization of a class/alias template or
            // template template parameter
            case Type::TemplateSpecialization:
            {
                auto* T = cast<TemplateSpecializationType>(type);
                auto name = T->getTemplateName();
                MRDOX_ASSERT(! name.isNull());
                NamedDecl* ND = name.getAsTemplateDecl();
                // if this is a specialization of a alias template,
                // the canonical type will be the named type. in such cases,
                // we will use the template name. otherwise, we use the
                // canonical type whenever possible.
                if(! T->isTypeAlias())
                {
                    auto* CT = qt.getCanonicalType().getTypePtrOrNull();
                    if(auto* ICT = dyn_cast_or_null<InjectedClassNameType>(CT))
                        ND = ICT->getDecl();
                    else if(auto* RT = dyn_cast_or_null<RecordType>(CT))
                        ND = RT->getDecl();
                }
                *inner = makeTypeInfo<SpecializationTypeInfo>(
                    ND, quals, T->template_arguments());
                break;
            }
            case Type::Record:
            {
                auto* T = cast<RecordType>(type);
                RecordDecl* RD = T->getDecl();
                // if this is an instantiation of a class template,
                // create a SpecializationTypeInfo & extract the template arguments
                if(auto* CTSD = dyn_cast<ClassTemplateSpecializationDecl>(RD))
                    *inner = makeTypeInfo<SpecializationTypeInfo>(
                        CTSD, quals, CTSD->getTemplateArgs().asArray());
                else
                    *inner = makeTypeInfo<TagTypeInfo>(RD, quals);
                break;
            }
            // enum types, as well as injected class names
            // within a class template (or specializations thereof)
            case Type::InjectedClassName:
            case Type::Enum:
            {
                *inner = makeTypeInfo<TagTypeInfo>(
                    type->getAsTagDecl(), quals);
                break;
            }
            // typedef/alias type
            case Type::Typedef:
            {
                auto* T = cast<TypedefType>(type);
                *inner = makeTypeInfo<TagTypeInfo>(
                    T->getDecl(), quals);
                break;
            }
            case Type::TemplateTypeParm:
            {
                auto* T = cast<TemplateTypeParmType>(type);
                auto I = std::make_unique<BuiltinTypeInfo>();
                I->CVQualifiers = convertToQualifierKind(quals);
                if(auto* D = T->getDecl())
                {
                    // special case for implicit template parameters
                    // resulting from abbreviated function templates
                    if(D->isImplicit())
                        I->Name = "auto";
                    else if(auto* II = D->getIdentifier())
                        I->Name = II->getName();
                }
                *inner = std::move(I);
                break;
            }
            // this only seems to appear when a template parameter pack
            // from an enclosing template appears in a pack expansion which contains
            // a template parameter pack from an inner template. this does not seem
            // to appear when both packs are template arguments; e.g.
            // A<sizeof...(Ts), sizeof...(Us)> will use this, but A<A<Ts, Us>...> will not
            case Type::SubstTemplateTypeParmPack:
            {
                auto* T = cast<SubstTemplateTypeParmPackType>(type);
                *inner = makeTypeInfo<BuiltinTypeInfo>(
                    T->getIdentifier(), quals);
                break;
            }
            // builtin/unhandled type
            default:
            {
                auto I = std::make_unique<BuiltinTypeInfo>();
                I->CVQualifiers = convertToQualifierKind(quals);
                I->Name = getTypeAsString(
                    qt.withoutLocalFastQualifiers());
                *inner = std::move(I);
                break;
            }
            }
            // the terminal type must be BuiltinTypeInfo,
            // TagTypeInfo, or SpecializationTypeInfo
            MRDOX_ASSERT(
                (*inner)->isBuiltin() ||
                (*inner)->isTag() ||
                (*inner)->isSpecialization());

            // set whether the root node is a pack
            if(result)
                result->IsPackExpansion = is_pack_expansion;

            // if there is no nested-name-specifier for
            // the terminal type, then we are done
            if(! NNS)
                return result;

            // KRYSTIAN FIXME: nested-name-specifier on builtin type?
            visit(**inner, [&]<typename T>(T& t)
            {
                if constexpr(requires { t.ParentType; })
                {
                    // build the TypeInfo for the nested-name-specifier
                    // using the same mode used for this TypeInfo
                    buildParentTypeInfo(
                        t.ParentType,
                        NNS,
                        extract_mode);
                }
            });

            return result;
        }

    }

    /** Get the user-written `Decl` for a `Decl`

        Given a `Decl` `D`, `getInstantiatedFrom` will return the
        user-written `Decl` corresponding to `D`. For specializations
        which were implicitly instantiated, this will be whichever `Decl`
        was used as the pattern for instantiation.
    */
    Decl*
    getInstantiatedFrom(
        Decl* D)
    {
        if(! D)
            return nullptr;

        // KRYSTIAN TODO: support enums & aliases/alias templates
        return visit(D, [&]<typename DeclTy>(
            DeclTy* DT) -> Decl*
            {
                constexpr Decl::Kind kind =
                    DeclToKind<DeclTy>();

                // ------------------------------------------------

                // FunctionTemplate
                if constexpr(kind == Decl::FunctionTemplate)
                {
                    while(auto* MT = DT->getInstantiatedFromMemberTemplate())
                    {
                        if(DT->isMemberSpecialization())
                            break;
                        DT = MT;
                    }
                    return DT;
                }
                // FunctionDecl
                // CXXMethodDecl
                // CXXConstructorDecl
                // CXXConversionDecl
                // CXXDeductionGuideDecl
                // CXXDestructorDecl
                if constexpr(std::derived_from<DeclTy, FunctionDecl>)
                {
                    FunctionDecl* FD = DT;

                    const FunctionDecl* DD = nullptr;
                    if(FD->isDefined(DD, false))
                        FD = const_cast<FunctionDecl*>(DD);

                    if(MemberSpecializationInfo* MSI =
                        FD->getMemberSpecializationInfo())
                    {
                        if(! MSI->isExplicitSpecialization())
                            FD = cast<FunctionDecl>(
                                MSI->getInstantiatedFrom());
                    }
                    else if(FD->getTemplateSpecializationKind() !=
                        TSK_ExplicitSpecialization)
                    {
                        FD = FD->getFirstDecl();
                        if(auto* FTD = FD->getPrimaryTemplate())
                        {
                            FTD = cast<FunctionTemplateDecl>(
                                getInstantiatedFrom(FTD));
                            FD = FTD->getTemplatedDecl();
                        }
                    }

                    return FD;
                }

                // ------------------------------------------------

                // ClassTemplate
                if constexpr(kind == Decl::ClassTemplate)
                {
                    while(auto* MT = DT->getInstantiatedFromMemberTemplate())
                    {
                        if(DT->isMemberSpecialization())
                            break;
                        DT = MT;
                    }
                    return DT;
                }
                // ClassTemplatePartialSpecialization
                else if constexpr(kind == Decl::ClassTemplatePartialSpecialization)
                {
                    while(auto* MT = DT->getInstantiatedFromMember())
                    {
                        if(DT->isMemberSpecialization())
                            break;
                        DT = MT;
                    }
                }
                // ClassTemplateSpecialization
                else if constexpr(kind == Decl::ClassTemplateSpecialization)
                {
                    if(! DT->isExplicitSpecialization())
                    {
                        auto inst_from = DT->getSpecializedTemplateOrPartial();
                        if(auto* CTPSD = inst_from.template dyn_cast<
                            ClassTemplatePartialSpecializationDecl*>())
                        {
                            MRDOX_ASSERT(DT != CTPSD);
                            return getInstantiatedFrom(CTPSD);
                        }
                        // explicit instantiation declaration/definition
                        else if(auto* CTD = inst_from.template dyn_cast<
                            ClassTemplateDecl*>())
                        {
                            return getInstantiatedFrom(CTD);
                        }
                    }
                }
                // CXXRecordDecl
                // ClassTemplateSpecialization
                // ClassTemplatePartialSpecialization
                if constexpr(std::derived_from<DeclTy, CXXRecordDecl>)
                {
                    CXXRecordDecl* RD = DT;
                    while(MemberSpecializationInfo* MSI =
                        RD->getMemberSpecializationInfo())
                    {
                        // if this is a member of an explicit specialization,
                        // then we have the correct declaration
                        if(MSI->isExplicitSpecialization())
                            break;
                        RD = cast<CXXRecordDecl>(MSI->getInstantiatedFrom());
                    }
                    return RD;
                }

                // ------------------------------------------------

                // VarTemplate
                if constexpr(kind == Decl::VarTemplate)
                {
                    while(auto* MT = DT->getInstantiatedFromMemberTemplate())
                    {
                        if(DT->isMemberSpecialization())
                            break;
                        DT = MT;
                    }
                    return DT;
                }
                // VarTemplatePartialSpecialization
                else if constexpr(kind == Decl::VarTemplatePartialSpecialization)
                {
                    while(auto* MT = DT->getInstantiatedFromMember())
                    {
                        if(DT->isMemberSpecialization())
                            break;
                        DT = MT;
                    }
                }
                // VarTemplateSpecialization
                else if constexpr(kind == Decl::VarTemplateSpecialization)
                {
                    if(! DT->isExplicitSpecialization())
                    {
                        auto inst_from = DT->getSpecializedTemplateOrPartial();
                        if(auto* VTPSD = inst_from.template dyn_cast<
                            VarTemplatePartialSpecializationDecl*>())
                        {
                            MRDOX_ASSERT(DT != VTPSD);
                            return getInstantiatedFrom(VTPSD);
                        }
                        // explicit instantiation declaration/definition
                        else if(auto* VTD = inst_from.template dyn_cast<
                            VarTemplateDecl*>())
                        {
                            return getInstantiatedFrom(VTD);
                        }
                    }
                }
                // VarDecl
                // VarTemplateSpecialization
                // VarTemplatePartialSpecialization
                if constexpr(std::derived_from<DeclTy, VarDecl>)
                {
                    VarDecl* VD = DT;
                    while(MemberSpecializationInfo* MSI =
                        VD->getMemberSpecializationInfo())
                    {
                        if(MSI->isExplicitSpecialization())
                            break;
                        VD = cast<VarDecl>(MSI->getInstantiatedFrom());
                    }
                    return VD;
                }

                return DT;
            });
    }


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
                {
                    QualType QT = P->getDefaultArgument();
                    R->Default = buildTemplateArg(
                        TemplateArgument(QT, QT.isNull(), true));
                }
                return R;
            }
            else if constexpr(kind == Decl::NonTypeTemplateParm)
            {
                auto R = std::make_unique<NonTypeTParam>();
                R->Type = buildTypeInfo(P->getType());
                if(P->hasDefaultArgument())
                {
                    R->Default = buildTemplateArg(
                        TemplateArgument(P->getDefaultArgument(), true));
                }
                return R;
            }
            else if constexpr(kind == Decl::TemplateTemplateParm)
            {
                auto R = std::make_unique<TemplateTParam>();
                for(const NamedDecl* NP : *P->getTemplateParameters())
                {
                    R->Params.emplace_back(
                        buildTemplateParam(NP));
                }

                if(P->hasDefaultArgument())
                {
                    R->Default = buildTemplateArg(
                        P->getDefaultArgument().getArgument());
                }
                return R;
            }
            MRDOX_UNREACHABLE();
        });

        TP->Name = extractName(N);
        // KRYSTIAN NOTE: Decl::isParameterPack
        // returns true for function parameter packs
        TP->IsParameterPack =
            N->isTemplateParameterPack();

        return TP;
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
            MRDOX_UNREACHABLE();
            break;
        }
        // type
        case TemplateArgument::Type:
        {
            auto R = std::make_unique<TypeTArg>();
            QualType QT = A.getAsType();
            MRDOX_ASSERT(! QT.isNull());
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
                    getDependencyID(
                        getInstantiatedFrom(TD),
                        R->Template);
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
            MRDOX_UNREACHABLE();
        }
        return nullptr;
    }

    template<typename Range>
    void
    buildTemplateArgs(
        std::vector<std::unique_ptr<TArg>>& result,
        Range&& range)
    {
        for(const TemplateArgument& arg : range)
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
    parseTemplateArgs(
        TemplateInfo& I,
        ClassTemplateSpecializationDecl* spec)
    {
        if(Decl* primary = getInstantiatedFrom(spec->getSpecializedTemplate()))
            extractSymbolID(primary, I.Primary.emplace());
        // KRYSTIAN NOTE: when this is a partial specialization, we could use
        // ClassTemplatePartialSpecializationDecl::getTemplateArgsAsWritten
        const TypeSourceInfo* type_written = spec->getTypeAsWritten();
        // if the type as written is nullptr (it should never be), bail
        if(! type_written)
            return;
        auto args = type_written->getType()->getAs<
            TemplateSpecializationType>()->template_arguments();
        buildTemplateArgs(I.Args, args);
    }

    void
    parseTemplateArgs(
        TemplateInfo& I,
        VarTemplateSpecializationDecl* spec)
    {
        // unlike function and class templates, the USR generated
        // for variable templates differs from that of the VarDecl
        // returned by getTemplatedDecl. this might be a clang bug.
        // the USR of the templated VarDecl seems to be the correct one.
        if(auto* primary = dyn_cast<VarTemplateDecl>(
            getInstantiatedFrom(spec->getSpecializedTemplate())))
            extractSymbolID(primary->getTemplatedDecl(), I.Primary.emplace());
        const ASTTemplateArgumentListInfo* args_written = nullptr;
        // getTemplateArgsInfo returns nullptr for partial specializations,
        // so we use getTemplateArgsAsWritten if this is a partial specialization
        if(auto* partial = dyn_cast<VarTemplatePartialSpecializationDecl>(spec))
            args_written = partial->getTemplateArgsAsWritten();
        else
            args_written = spec->getTemplateArgsInfo();
        if(! args_written)
            return;
        auto args = args_written->arguments();
        buildTemplateArgs(I.Args, std::views::transform(
            args, [](auto& x) -> auto& { return x.getArgument(); }));
    }

    void
    parseTemplateArgs(
        TemplateInfo& I,
        FunctionTemplateSpecializationInfo* spec)
    {
        // KRYSTIAN NOTE: do we need to check I->Primary.has_value()?
        if(Decl* primary = getInstantiatedFrom(spec->getTemplate()))
            extractSymbolID(primary, I.Primary.emplace());
        // TemplateArguments is used instead of TemplateArgumentsAsWritten
        // because explicit specializations of function templates may have
        // template arguments deduced from their return type and parameters
        if(auto* args = spec->TemplateArguments)
            buildTemplateArgs(I.Args, args->asArray());
    }

    void
    parseTemplateArgs(
        TemplateInfo& I,
        const DependentFunctionTemplateSpecializationInfo* spec)
    {
        // set the ID of the primary template if there is one candidate
        if(auto candidates = spec->getCandidates();
            candidates.size() == 1)
        {
            if(Decl* primary = getInstantiatedFrom(candidates.front()))
                extractSymbolID(primary, I.Primary.emplace());
        }

        if(auto* args_written = spec->TemplateArgumentsAsWritten)
        {
            buildTemplateArgs(I.Args, std::views::transform(
                args_written->arguments(), [](auto& x) -> auto&
                {
                    return x.getArgument();
                }));
        }
    }

    void
    parseTemplateParams(
        TemplateInfo& I,
        const TemplateParameterList* TPL)
    {
        for(const NamedDecl* ND : *TPL)
        {
            I.Params.emplace_back(
                buildTemplateParam(ND));
        }
    }

    void
    applyDecayToParameters(
        const FunctionDecl* D)
    {
        // apply the type adjustments specified in [dcl.fct] p5
        // to ensure that the USR of the corresponding function matches
        // other declarations of the function that have parameters declared
        // with different top-level cv-qualifiers.
        // this needs to be done prior to USR generation for the function
        for(ParmVarDecl* P : D->parameters())
            P->setType(context_.getSignatureParameterType(P->getType()));
    }

    void
    parseRawComment(
        std::unique_ptr<Javadoc>& javadoc,
        Decl const* D)
    {
        // VFALCO investigate whether we can use
        // ASTContext::getCommentForDecl instead
        RawComment* RC =
            D->getASTContext().getRawCommentForDeclNoCache(D);
        parseJavadoc(javadoc, RC, D, config_, diags_);
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
                const DeclContext* parent = ND->getDeclContext();
                do
                {
                    switch(parent->getDeclKind())
                    {
                    case Decl::Namespace:
                    case Decl::Enum:
                    case Decl::CXXRecord:
                    case Decl::ClassTemplateSpecialization:
                    case Decl::ClassTemplatePartialSpecialization:
                        parents.push_back(cast<NamedDecl>(parent));
                        break;
                    default:
                        break;
                    }
                }
                while((parent = parent->getParent()));

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

        const PresumedLoc loc =
            source_.getPresumedLoc(D->getBeginLoc());

        MRDOX_ASSERT(loc.isValid());

        file_ = files::makePosixStyle(loc.getFilename());

        // skip system header
        if(source_.isInSystemHeader(D->getLocation()))
            return false;

        auto [it, inserted] = fileFilter_.emplace(
            loc.getIncludeLoc().getRawEncoding(),
            FileFilter());

        FileFilter& ff = it->second;

        // file has not been previously visited
        if(inserted)
            ff.include = config_.shouldExtractFromFile(file_, ff.prefix);

        // don't extract if the declaration is in a file
        // that should not be visited
        if(! ff.include)
            return false;
        // VFALCO we could assert that the prefix
        //        matches and just lop off the
        //        first ff.prefix.size() characters.
        path::replace_path_prefix(file_, ff.prefix, "");

        // KRYSTIAN FIXME: once set, this never gets reset
        isFileInRootDir_ = true;

        return true;
    }

    bool
    shouldExtract(
        const Decl* D)
    {
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
            MRDOX_ASSERT(isa<CXXConversionDecl>(D));
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
            MRDOX_UNREACHABLE();
        }
        return result;
    }

    //------------------------------------------------

    void
    getParentNamespaces(
        Info& I,
        Decl* D)
    {
        // this function should be called once per Info
        MRDOX_ASSERT(I.Namespace.empty());

        Decl* child = D;
        SymbolID child_id = I.id;
        DeclContext* parent_context = child->getDeclContext();
        do
        {
            Decl* parent = cast<Decl>(parent_context);
            SymbolID parent_id = extractSymbolID(parent);
            switch(parent_context->getDeclKind())
            {
            // the TranslationUnit DeclContext is the global namespace;
            // it uses SymbolID::zero and should *always* exist
            case Decl::TranslationUnit:
            {
                parent_id = SymbolID::zero;
                auto [P, created] = getOrCreateInfo<
                    NamespaceInfo>(parent_id);
                emplaceChild(P, child_id);
                break;
            }
            case Decl::Namespace:
            {
                auto [P, created] = getOrCreateInfo<
                    NamespaceInfo>(parent_id);
                buildNamespace(P, created, cast<NamespaceDecl>(parent));
                emplaceChild(P, child_id);
                break;
            }
            // special case for an explicit specializations of
            // a member of an implicit instantiation.
            case Decl::ClassTemplateSpecialization:
            case Decl::ClassTemplatePartialSpecialization:
            if(auto* S = dyn_cast<ClassTemplateSpecializationDecl>(parent_context);
                S && S->getSpecializationKind() == TSK_ImplicitInstantiation)
            {
                // KRYSTIAN FIXME: i'm pretty sure DeclContext::getDeclKind()
                // will never be Decl::ClassTemplatePartialSpecialization for
                // implicit instantiations; instead, the ClassTemplatePartialSpecializationDecl
                // is accessible through S->getSpecializedTemplateOrPartial
                // if the implicit instantiation used a partially specialized template,
                MRDOX_ASSERT(parent_context->getDeclKind() !=
                    Decl::ClassTemplatePartialSpecialization);

                auto [P, created] = getOrCreateInfo<
                    SpecializationInfo>(parent_id);
                buildSpecialization(P, created, S);
                // KRYSTIAN FIXME: extract primary/specialized ID properly
                emplaceChild(P, SpecializedMember(child_id, child_id));
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
                    RecordInfo>(parent_id);
                buildRecord(P, created, cast<CXXRecordDecl>(parent));
                emplaceChild(P, child_id);
                break;
            }
            // KRYSTIAN FIXME: we may need to handle
            // enumerators separately at some point
            // case Decl::Enum:
            default:
                // we consider all other DeclContexts to be "transparent"
                // and do not include them in the list of parents.
                continue;
            }
            I.Namespace.emplace_back(parent_id);
            child = parent;
            child_id = parent_id;
        }
        while((parent_context = parent_context->getParent()));
    }

    template<
        typename InfoTy,
        typename Child>
    void
    emplaceChild(
        InfoTy& I,
        Child&& C)
    {
        if constexpr(requires { I.Specializations; })
        {
            auto& S = I.Members;
            if(Info* child = getInfo(C);
                child && child->isSpecialization())
            {
                if(std::find(S.begin(), S.end(), C) == S.end())
                    S.emplace_back(C);
                return;
            }
        }
        auto& M = I.Members;
        if(std::find(M.begin(), M.end(), C) == M.end())
            M.emplace_back(C);
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

        NamedDecl* PD = cast<NamedDecl>(
            getInstantiatedFrom(D));

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

    void
    buildNamespace(
        NamespaceInfo& I,
        bool created,
        NamespaceDecl* D)
    {
        if(! created)
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
        parseRawComment(I.javadoc, D);
        addSourceLocation(I, getLine(D),
            D->isThisDeclarationADefinition());

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
                I.Bases.emplace_back(
                    // the extraction of the base type is
                    // performed in direct dependency mode
                    buildTypeInfo(
                        B.getType(),
                        ExtractMode::DirectDependency),
                    convertToAccessKind(
                        B.getAccessSpecifier()),
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
        parseRawComment(I.javadoc, D);
        addSourceLocation(I, getLine(D),
            D->isThisDeclarationADefinition());

        if(! created)
            return;

        I.Name = extractName(D);

        I.Scoped = D->isScoped();

        if(D->isFixed())
            I.UnderlyingType = buildTypeInfo(
                D->getIntegerType());

        for(const EnumConstantDecl* E : D->enumerators())
        {
            auto& M = I.Members.emplace_back(
                E->getNameAsString());

            buildExprInfo(
                M.Initializer,
                E->getInitExpr(),
                E->getInitVal());

            parseRawComment(I.Members.back().javadoc, E);
        }

        getParentNamespaces(I, D);
    }

    //------------------------------------------------

    void
    buildTypedef(
        TypedefInfo& I,
        bool created,
        TypedefNameDecl* D)
    {
        parseRawComment(I.javadoc, D);
        // KRYSTIAN FIXME: we currently treat typedef/alias
        // declarations as having a single definition; however,
        // such declarations are never definitions and can
        // be redeclared multiple times (even in the same scope)
        addSourceLocation(I, getLine(D), true);

        if(! created)
            return;

        I.Name = extractName(D);

        I.Type = buildTypeInfo(
            D->getUnderlyingType());

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
        parseRawComment(I.javadoc, D);
        addSourceLocation(I, getLine(D),
            D->isThisDeclarationADefinition());

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
        parseRawComment(I.javadoc, D);
        // fields (i.e. non-static data members)
        // cannot have multiple declarations
        addSourceLocation(I, getLine(D), true);

        if(! created)
            return;

        I.Name = extractName(D);

        I.Type = buildTypeInfo(D->getType());

        I.IsMutable = D->isMutable();

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
        parseRawComment(I.javadoc, D);
        addSourceLocation(I, getLine(D),
            D->isThisDeclarationADefinition());

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
        if(auto const* FP = D->getType()->template getAs<FunctionProtoType>())
            I.specs0.hasTrailingReturn |= FP->hasTrailingReturn();
        I.specs0.constexprKind |=
            convertToConstexprKind(
                D->getConstexprKind());
        I.specs0.exceptionSpec |=
            convertToNoexceptKind(
                D->getExceptionSpecType());
        I.specs0.overloadedOperator |=
            convertToOperatorKind(
                D->getOverloadedOperator());
        I.specs0.storageClass |=
            convertToStorageClassKind(
                D->getStorageClass());

        I.specs1.isNodiscard |= D->template hasAttr<WarnUnusedResultAttr>();

        //
        // CXXMethodDecl
        //
        if constexpr(std::derived_from<DeclTy, CXXMethodDecl>)
        {
            I.specs0.isVirtual |= D->isVirtual();
            I.specs0.isVirtualAsWritten |= D->isVirtualAsWritten();
            I.specs0.isPure |= D->isPure();
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

        //
        // CXXDeductionGuideDecl
        //
        if constexpr(std::derived_from<DeclTy, CXXDeductionGuideDecl>)
        {
            I.specs1.explicitSpec |=
                convertToExplicitKind(
                    D->getExplicitSpecifier());
        }

        if(! created)
            return;

        I.Name = extractName(D);

        I.Class = convertToFunctionClass(
            D->getDeclKind());

        for(const ParmVarDecl* P : D->parameters())
        {
            I.Params.emplace_back(
                buildTypeInfo(P->getOriginalType()),
                P->getNameAsString(),
                getSourceCode(P->getDefaultArgRange()));
        }

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
    buildFriend(
        FriendDecl* D)
    {
        if(NamedDecl* ND = D->getFriendDecl())
        {
            // D does not name a type
            if(FunctionDecl* FD = dyn_cast<FunctionDecl>(ND))
            {
                if(! shouldExtract(FD))
                    return;

                SymbolID id;
                if(! extractSymbolID(FD, id))
                    return;
                auto [I, created] = getOrCreateInfo<FunctionInfo>(id);
                buildFunction(I, created, FD);

                const DeclContext* DC = D->getDeclContext();
                const auto* RD = dyn_cast<CXXRecordDecl>(DC);
                // the semantic DeclContext of a FriendDecl must be a class
                MRDOX_ASSERT(RD);
                SymbolID parent_id = extractSymbolID(RD);
                if(Info* parent = getInfo(parent_id))
                {
                    MRDOX_ASSERT(parent->isRecord());
                    parent->Implicit &= currentMode() != ExtractMode::Normal;
                    static_cast<RecordInfo*>(parent)->Friends.emplace_back(I.id);
                }
                return;
            }
            if(FunctionTemplateDecl* FT = dyn_cast<FunctionTemplateDecl>(ND))
            {
                // VFALCO TODO
                (void)FT;
                return;
            }
            if(ClassTemplateDecl* CT = dyn_cast<ClassTemplateDecl>(ND))
            {
                // VFALCO TODO
                (void)CT;
                return;
            }

            MRDOX_UNREACHABLE();
        }
        else if(TypeSourceInfo* TS = D->getFriendType())
        {
            (void)TS;
            return;
        }
        else
        {
            MRDOX_UNREACHABLE();
        }
        return;
    }

    //------------------------------------------------

    // namespaces
    bool traverse(NamespaceDecl*);

    // enums
    bool traverse(EnumDecl*);

    // KRYSTIAN TODO: friends are a can of worms
    // we do not wish to open just yet
    bool traverse(FriendDecl*);

    // non-static data members
    bool traverse(FieldDecl*);

    template<std::derived_from<CXXRecordDecl> CXXRecordTy>
    bool traverse(CXXRecordTy*, ClassTemplateDecl* = nullptr);

    template<std::derived_from<VarDecl> VarTy>
    bool traverse(VarTy*, VarTemplateDecl* = nullptr);

    template<std::derived_from<FunctionDecl> FunctionTy>
    bool traverse(FunctionTy*, FunctionTemplateDecl* = nullptr);

    template<std::derived_from<TypedefNameDecl> TypedefNameTy>
    bool traverse(TypedefNameTy*, TypeAliasTemplateDecl* = nullptr);

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
    auto traverse(Decl* D, Args&&...);

    template<typename... Args>
    bool traverseDecl(Decl* D, Args&&... args);
    bool traverseContext(DeclContext* D);
};

//------------------------------------------------
// NamespaceDecl

bool
ASTVisitor::
traverse(NamespaceDecl* D)
{
    if(! shouldExtract(D))
        return true;

    if(D->isAnonymousNamespace() &&
        config_->anonymousNamespaces !=
            ConfigImpl::SettingsImpl::ExtractPolicy::Always)
    {
        // always skip anonymous namespaces if so configured
        if(config_->anonymousNamespaces ==
            ConfigImpl::SettingsImpl::ExtractPolicy::Never)
            return true;

        // otherwise, skip extraction if this isn't a dependency
        // KRYSTIAN FIXME: is this correct? a namespace should not
        // be extracted as a dependency (until namespace aliases and
        // using directives are supported)
        if(currentMode() == ExtractMode::Normal)
            return true;
    }

    SymbolID id;
    if(! extractSymbolID(D, id))
        return true;
    auto [I, created] = getOrCreateInfo<NamespaceInfo>(id);

    buildNamespace(I, created, D);
    return traverseContext(D);
}

//------------------------------------------------
// EnumDecl

bool
ASTVisitor::
traverse(EnumDecl* D)
{
    if(! shouldExtract(D))
        return true;

    SymbolID id;
    if(! extractSymbolID(D, id))
        return false;
    auto [I, created] = getOrCreateInfo<EnumInfo>(id);
    I.Access = convertToAccessKind(D->getAccessUnsafe());

    buildEnum(I, created, D);
    return true;
}

//------------------------------------------------
// FieldDecl

bool
ASTVisitor::
traverse(FieldDecl* D)
{
    if(! shouldExtract(D))
        return true;

    SymbolID id;
    if(! extractSymbolID(D, id))
        return false;
    auto [I, created] = getOrCreateInfo<FieldInfo>(id);
    I.Access = convertToAccessKind(D->getAccessUnsafe());

    buildField(I, created, D);
    return true;
}

//------------------------------------------------
// FriendDecl

bool
ASTVisitor::
traverse(FriendDecl* D)
{
    buildFriend(D);
    return true;
}

//------------------------------------------------

template<std::derived_from<CXXRecordDecl> CXXRecordTy>
bool
ASTVisitor::
traverse(CXXRecordTy* D,
    ClassTemplateDecl* CTD)
{
    if(! shouldExtract(D))
        return true;

    SymbolID id;
    if(! extractSymbolID(D, id))
        return false;

    auto [I, created] = getOrCreateInfo<RecordInfo>(id);

    AccessSpecifier access = D->getAccessUnsafe();
    // CTD is the specialized template if D is a partial or
    // explicit specialization, and the described template otherwise
    if(CTD)
    {
        // use the access of the described/specialized template
        access = CTD->getAccessUnsafe();

        I.Template = std::make_unique<TemplateInfo>();
        // if D is a partial/explicit specialization, extract the template arguments
        if(auto* CTSD = dyn_cast<ClassTemplateSpecializationDecl>(D))
        {
            parseTemplateArgs(*I.Template, CTSD);
            // extract the template parameters if this is a partial specialization
            if(auto* CTPSD = dyn_cast<ClassTemplatePartialSpecializationDecl>(D))
                parseTemplateParams(*I.Template,
                    CTPSD->getTemplateParameters());
        }
        else
        {
            // otherwise, extract the template parameter list from CTD
            parseTemplateParams(*I.Template,
                CTD->getTemplateParameters());
        }
    }

    I.Access = convertToAccessKind(access);

    buildRecord(I, created, D);
    return traverseContext(D);
}

template<std::derived_from<VarDecl> VarTy>
bool
ASTVisitor::
traverse(VarTy* D,
    VarTemplateDecl* VTD)
{
    if(! shouldExtract(D))
        return true;

    SymbolID id;
    if(! extractSymbolID(D, id))
        return false;

    auto [I, created] = getOrCreateInfo<VariableInfo>(id);

    AccessSpecifier access = D->getAccessUnsafe();
    // VTD is the specialized template if D is a partial or
    // explicit specialization, and the described template otherwise
    if(VTD)
    {
        // use the access of the described/specialized template
        access = VTD->getAccessUnsafe();

        I.Template = std::make_unique<TemplateInfo>();
        // if D is a partial/explicit specialization, extract the template arguments
        if(auto* VTSD = dyn_cast<VarTemplateSpecializationDecl>(D))
        {
            parseTemplateArgs(*I.Template, VTSD);
            // extract the template parameters if this is a partial specialization
            if(auto* VTPSD = dyn_cast<VarTemplatePartialSpecializationDecl>(D))
                parseTemplateParams(*I.Template,
                    VTPSD->getTemplateParameters());
        }
        else
        {
            // otherwise, extract the template parameter list from VTD
            parseTemplateParams(*I.Template,
                VTD->getTemplateParameters());
        }
    }

    I.Access = convertToAccessKind(access);

    buildVariable(I, created, D);
    return true;
}


template<std::derived_from<FunctionDecl> FunctionTy>
bool
ASTVisitor::
traverse(FunctionTy* D,
    FunctionTemplateDecl* FTD)
{
    if(! shouldExtract(D))
        return true;

    SymbolID id;
    if(! extractSymbolID(D, id))
        return false;

    auto [I, created] = getOrCreateInfo<FunctionInfo>(id);

    AccessSpecifier access = D->getAccessUnsafe();
    // D is the templated declaration if FTD is non-null
    if(FTD || D->isFunctionTemplateSpecialization())
    {
        I.Template = std::make_unique<TemplateInfo>();

        if(FTD)
        {
            access = FTD->getAccessUnsafe();
            parseTemplateParams(*I.Template,
                FTD->getTemplateParameters());
        }
        else if(auto* FTSI = D->getTemplateSpecializationInfo())
        {
            parseTemplateArgs(*I.Template, FTSI);
        }
        else if(auto* DFTSI = D->getDependentSpecializationInfo())
        {
            parseTemplateArgs(*I.Template, DFTSI);
        }
    }

    I.Access = convertToAccessKind(access);

    buildFunction(I, created, D);
    return true;
}

template<std::derived_from<TypedefNameDecl> TypedefNameTy>
bool
ASTVisitor::
traverse(TypedefNameTy* D,
    TypeAliasTemplateDecl* ATD)
{
    if(! shouldExtract(D))
        return true;

    SymbolID id;
    if(! extractSymbolID(D, id))
        return false;

    auto [I, created] = getOrCreateInfo<TypedefInfo>(id);

    if(isa<TypeAliasDecl>(D))
        I.IsUsing = true;

    AccessSpecifier access = D->getAccessUnsafe();

    if(ATD)
    {
        access = ATD->getAccessUnsafe();

        I.Template = std::make_unique<TemplateInfo>();
        parseTemplateParams(*I.Template,
            ATD->getTemplateParameters());
    }

    I.Access = convertToAccessKind(access);

    buildTypedef(I, created, D);
    return true;
}

//------------------------------------------------

template<typename... Args>
auto
ASTVisitor::
traverse(Decl* D, Args&&...)
{
    if(auto* DC = dyn_cast<DeclContext>(D))
        traverseContext(DC);
}

//------------------------------------------------

template<typename... Args>
bool
ASTVisitor::
traverseDecl(
    Decl* D,
    Args&&... args)
{
    MRDOX_ASSERT(D);

    if(D->isInvalidDecl() || D->isImplicit())
        return true;

    SymbolFilter::FilterScope scope(symbolFilter_);

    visit(D, [&]<typename DeclTy>(DeclTy* DD)
    {
        // only ClassTemplateDecl, FunctionTemplateDecl, VarTemplateDecl,
        // and TypeAliasDecl are derived from RedeclarableTemplateDecl.
        // note that this doesn't include ConceptDecl.
        if constexpr(std::derived_from<DeclTy,
            RedeclarableTemplateDecl>)
        {
            // call traverseDecl so traverse is called with
            // a pointer to the most derived type of the templated Decl
            traverseDecl(DD->getTemplatedDecl(), DD);
        }
        else if constexpr(std::derived_from<DeclTy,
            ClassTemplateSpecializationDecl>)
        {
            traverse(DD, DD->getSpecializedTemplate());
        }
        else if constexpr(std::derived_from<DeclTy,
            VarTemplateSpecializationDecl>)
        {
            traverse(DD, DD->getSpecializedTemplate());
        }
        else
        {
            traverse(DD, std::forward<Args>(args)...);
        }
    });

    return true;
}

bool
ASTVisitor::
traverseContext(
    DeclContext* D)
{
    MRDOX_ASSERT(D);
    for(auto* C : D->decls())
        if(! traverseDecl(C))
            return false;
    return true;
}

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
        MRDOX_ASSERT(! sema_);
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
        MRDOX_ASSERT(sema_);

        // initialize the diagnostics reporter first
        // so errors prior to traversal are reported
        Diagnostics diags;

        SourceManager& source = Context.getSourceManager();
        // get the name of the translation unit.
        // will be std::nullopt_t if it isn't a file
        std::optional<llvm::SmallString<128>> file_name =
            source.getNonBuiltinFilenameForID(source.getMainFileID());
        // KRYSTIAN NOTE: should we report anything here?
        if(! file_name)
            return;

        // skip the translation unit if configured to do so
        if(! config_.shouldVisitTU(
            convert_to_slash(*file_name)))
            return;

        ASTVisitor visitor(
            config_,
            diags,
            compiler_,
            Context,
            *sema_);

        // traverse the translation unit
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

    void
    ExecuteAction() override
    {
        CompilerInstance& CI = getCompilerInstance();
        if(! CI.hasPreprocessor())
            return;

        if(! CI.hasSema())
            CI.createSema(getTranslationUnitKind(), nullptr);

        ParseAST(
            CI.getSema(),
            false, // ShowStats
            true); // SkipFunctionBodies
    }

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

} // mrdox
} // clang
