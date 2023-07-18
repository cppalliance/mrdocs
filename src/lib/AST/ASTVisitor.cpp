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
#include <mrdox/Metadata.hpp>
#include <clang/AST/AST.h>
#include <clang/AST/Attr.h>
#include <clang/AST/DeclFriend.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Index/USRGeneration.h>
#include <clang/Lex/Lexer.h>
#include <clang/Parse/ParseAST.h>
#include <clang/Sema/SemaConsumer.h>
#include <llvm/ADT/Hashing.h>
#include <llvm/ADT/StringExtras.h>
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

using InfoPtr = std::unique_ptr<Info>;

struct InfoPtrHasher
{
    using is_transparent = void;

    std::size_t operator()(
        const InfoPtr& I) const
    {
        // the info set should never contain nullptrs
        MRDOX_ASSERT(I);
        return std::hash<SymbolID>()(I->id);
    }

    std::size_t operator()(
        const SymbolID& id) const
    {
        return std::hash<SymbolID>()(id);
    }
};

struct InfoPtrEqual
{
    using is_transparent = void;

    bool operator()(
        const InfoPtr& a,
        const InfoPtr& b) const
    {
        MRDOX_ASSERT(a && b);
        if(a == b)
            return true;
        return a->id == b->id;
    }

    bool operator()(
        const InfoPtr& a,
        const SymbolID& b) const
    {
        MRDOX_ASSERT(a);
        return a->id == b;
    }

    bool operator()(
        const SymbolID& a,
        const InfoPtr& b) const
    {
        MRDOX_ASSERT(b);
        return b->id == a;
    }
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

    std::unordered_set<
        InfoPtr,
        InfoPtrHasher,
        InfoPtrEqual> info_;

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

    // KRYSTIAN FIXME: this is terrible
    bool forceExtract_ = false;

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

    auto& results()
    {
        return info_;
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
    InfoTy&
    createInfo(const SymbolID& id)
    {
        auto [it, inserted] = info_.emplace(
            std::make_unique<InfoTy>(id));
        MRDOX_ASSERT(inserted);
        return static_cast<InfoTy&>(*it->get());
    }

    template<typename InfoTy>
    std::pair<InfoTy&, bool>
    getOrCreateInfo(const SymbolID& id)
    {
        if(Info* info = getInfo(id))
        {
            MRDOX_ASSERT(info->Kind == InfoTy::kind_id);
            return {static_cast<InfoTy&>(*info), false};
        }
        return {createInfo<InfoTy>(id), true};
    }

    Info&
    getOrBuildInfo(Decl* D)
    {
        SymbolID id = extractSymbolID(D);
        if(Info* info = getInfo(id))
            return *info;

        // KRYSTIAN FIXME: this is terrible
        bool force = forceExtract_;
        forceExtract_ = true;
        traverseDecl(D);
        forceExtract_ = force;

        MRDOX_ASSERT(getInfo(id));
        return *getInfo(id);
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
        NamedDecl* N,
        unsigned quals)
    {
        auto I = std::make_unique<TypeInfoTy>();
        I->CVQualifiers = convertToQualifierKind(quals);
        if(! N)
            return I;
        if(const auto* II = N->getIdentifier())
            I->Name = II->getName();
        // do not generate references to implicit declarations,
        // template template parameters, or builtin templates
        if(! N->isImplicit() &&
            ! isa<TemplateTemplateParmDecl>(N) &&
            ! isa<BuiltinTemplateDecl>(N))
        {
            N = cast<NamedDecl>(getInstantiatedFrom(N));
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

            extractSymbolID(N, I->id);
            getOrBuildInfo(N);
        }
        return I;
    }

    std::unique_ptr<TypeInfo>
    buildTypeInfo(
        const NestedNameSpecifier* N)
    {
        if(N)
        {
            if(const auto* T = N->getAsType())
                return buildTypeInfo(QualType(T, 0));
            if(const auto* I = N->getAsIdentifier())
            {
                auto R = std::make_unique<TagTypeInfo>();
                R->ParentType = buildTypeInfo(N->getPrefix());
                R->Name = I->getName();
                return R;
            }
        }
        return nullptr;
    }

    // KRYSTIAN FIXME: something is broken here w.r.t
    // qualified names, but i'm not sure what exactly
    std::unique_ptr<TypeInfo>
    buildTypeInfo(
        QualType qt,
        unsigned quals = 0)
    {
        qt.addFastQualifiers(quals);
        // should never be called for a QualType
        // that has no Type pointer
        MRDOX_ASSERT(! qt.isNull());
        const Type* type = qt.getTypePtr();
        quals = qt.getLocalFastQualifiers();
        switch(qt->getTypeClass())
        {
        // parenthesized types
        case Type::Paren:
        {
            auto* T = cast<ParenType>(type);
            return buildTypeInfo(
                T->getInnerType(), quals);
        }
        case Type::MacroQualified:
        {
            auto* T = cast<MacroQualifiedType>(type);
            return buildTypeInfo(
                T->getUnderlyingType(), quals);
        }
        // type with __atribute__
        case Type::Attributed:
        {
            auto* T = cast<AttributedType>(type);
            return buildTypeInfo(
                T->getModifiedType(), quals);
        }
        // adjusted and decayed types
        case Type::Decayed:
        case Type::Adjusted:
        {
            auto* T = cast<AdjustedType>(type);
            return buildTypeInfo(
                T->getOriginalType(), quals);
        }
        // using declarations
        case Type::Using:
        {
            auto* T = cast<UsingType>(type);
            // look through the using declaration and
            // use the the type from the referenced declaration
            return buildTypeInfo(
                T->getUnderlyingType(), quals);
        }
        // pointers
        case Type::Pointer:
        {
            auto* T = cast<PointerType>(type);
            auto I = std::make_unique<PointerTypeInfo>();
            I->PointeeType = buildTypeInfo(
                T->getPointeeType());
            I->CVQualifiers = convertToQualifierKind(quals);
            return I;
        }
        // references
        case Type::LValueReference:
        {
            auto* T = cast<LValueReferenceType>(type);
            auto I = std::make_unique<LValueReferenceTypeInfo>();
            I->PointeeType = buildTypeInfo(
                T->getPointeeType());
            return I;
        }
        case Type::RValueReference:
        {
            auto* T = cast<RValueReferenceType>(type);
            auto I = std::make_unique<RValueReferenceTypeInfo>();
            I->PointeeType = buildTypeInfo(
                T->getPointeeType());
            return I;
        }
        // pointer to members
        case Type::MemberPointer:
        {
            auto* T = cast<MemberPointerType>(type);
            auto I = std::make_unique<MemberPointerTypeInfo>();
            I->PointeeType = buildTypeInfo(
                T->getPointeeType());
            I->ParentType = buildTypeInfo(
                QualType(T->getClass(), 0));
            I->CVQualifiers = convertToQualifierKind(quals);
            return I;
        }
        // pack expansion
        case Type::PackExpansion:
        {
            auto* T = cast<PackExpansionType>(type);
            auto I = std::make_unique<PackTypeInfo>();
            I->PatternType = buildTypeInfo(T->getPattern());
            return I;
        }
        // KRYSTIAN NOTE: we don't handle FunctionNoProto here,
        // and it's unclear if we should. we should not encounter
        // such types in c++ (but it might be possible?)
        // functions
        case Type::FunctionProto:
        {
            auto* T = cast<FunctionProtoType>(type);
            auto I = std::make_unique<FunctionTypeInfo>();
            I->ReturnType = buildTypeInfo(
                T->getReturnType());
            for(QualType PT : T->getParamTypes())
                I->ParamTypes.emplace_back(
                    buildTypeInfo(PT));
            I->RefQualifier = convertToReferenceKind(
                T->getRefQualifier());
            I->CVQualifiers = convertToQualifierKind(
                T->getMethodQuals().getFastQualifiers());
            I->ExceptionSpec = convertToNoexceptKind(
                T->getExceptionSpecType());
            return I;
        }
        // KRYSTIAN FIXME: do we handle variables arrays?
        // they can only be created within function scope
        // arrays
        case Type::IncompleteArray:
        {
            auto* T = cast<IncompleteArrayType>(type);
            auto I = std::make_unique<ArrayTypeInfo>();
            I->ElementType = buildTypeInfo(
                T->getElementType());
            return I;
        }
        case Type::ConstantArray:
        {
            auto* T = cast<ConstantArrayType>(type);
            auto I = std::make_unique<ArrayTypeInfo>();
            I->ElementType = buildTypeInfo(
                T->getElementType());
            // KRYSTIAN FIXME: this is broken; cannonical
            // constant array types never have a size expression
            buildExprInfo(I->Bounds,
                T->getSizeExpr(), T->getSize());
            return I;
        }
        case Type::DependentSizedArray:
        {
            auto* T = cast<DependentSizedArrayType>(type);
            auto I = std::make_unique<ArrayTypeInfo>();
            I->ElementType = buildTypeInfo(
                T->getElementType());
            buildExprInfo(I->Bounds,
                T->getSizeExpr());
            return I;
        }
        case Type::Auto:
        {
            auto* T = cast<AutoType>(type);
            QualType deduced = T->getDeducedType();
            // KRYSTIAN NOTE: we don't use isDeduced because it will
            // return true if the type is dependent
            // if the type has been deduced, use the deduced type
            if(! deduced.isNull())
                return buildTypeInfo(deduced);
            auto I = std::make_unique<BuiltinTypeInfo>();
            I->Name = getTypeAsString(
                qt.withoutLocalFastQualifiers());
            I->CVQualifiers = convertToQualifierKind(quals);
            return I;
        }
        case Type::DeducedTemplateSpecialization:
        {
            auto* T = cast<DeducedTemplateSpecializationType>(type);
            if(T->isDeduced())
                return buildTypeInfo(T->getDeducedType());
            auto I = makeTypeInfo<TagTypeInfo>(
                T->getTemplateName().getAsTemplateDecl(), quals);
            return I;
        }
        // elaborated type specifier or
        // type with nested name specifier
        case Type::Elaborated:
        {
            auto* T = cast<ElaboratedType>(type);
            auto I = buildTypeInfo(
                T->getNamedType(), quals);
            // ignore elaborated-type-specifiers
            if(auto kw = T->getKeyword();
                kw != ElaboratedTypeKeyword::ETK_Typename &&
                kw != ElaboratedTypeKeyword::ETK_None)
                return I;
            switch(I->Kind)
            {
            case TypeKind::Tag:
                static_cast<TagTypeInfo&>(*I).ParentType =
                    buildTypeInfo(T->getQualifier());
                break;
            case TypeKind::Specialization:
                static_cast<SpecializationTypeInfo&>(*I).ParentType =
                    buildTypeInfo(T->getQualifier());
                break;
            case TypeKind::Builtin:
                // KRYSTIAN FIXME: is this correct?
                break;
            default:
                MRDOX_UNREACHABLE();
            };
            return I;
        }
        // qualified dependent name with template keyword
        case Type::DependentTemplateSpecialization:
        {
            auto* T = cast<DependentTemplateSpecializationType>(type);
            auto I = makeTypeInfo<SpecializationTypeInfo>(
                T->getIdentifier(), quals);
            I->ParentType = buildTypeInfo(
                T->getQualifier());
            buildTemplateArgs(I->TemplateArgs, T->template_arguments());
            return I;
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
                if(auto* ICT = dyn_cast_or_null<
                    InjectedClassNameType>(CT))
                {
                    ND = ICT->getDecl();
                }
                if(auto* RT = dyn_cast_or_null<
                    RecordType>(CT))
                {
                    ND = RT->getDecl();
                }
            }
            auto I = makeTypeInfo<SpecializationTypeInfo>(ND, quals);
            buildTemplateArgs(I->TemplateArgs, T->template_arguments());
            return I;
        }
        // dependent typename-specifier
        case Type::DependentName:
        {
            auto* T = cast<DependentNameType>(type);
            auto I = makeTypeInfo<TagTypeInfo>(
                T->getIdentifier(), quals);
            I->ParentType = buildTypeInfo(
                T->getQualifier());
            return I;
        }
        case Type::Record:
        {
            auto* T = cast<RecordType>(type);
            RecordDecl* RD = T->getDecl();
            // if this is an instantiation of a class template,
            // create a SpecializationTypeInfo & extract the template arguments
            if(auto* CTSD = dyn_cast<ClassTemplateSpecializationDecl>(RD))
            {
                auto I = makeTypeInfo<SpecializationTypeInfo>(CTSD, quals);
                buildTemplateArgs(I->TemplateArgs,
                    CTSD->getTemplateArgs().asArray());
                return I;
            }
            return makeTypeInfo<TagTypeInfo>(RD, quals);
        }
        // enum types, as well as injected class names
        // within a class template (or specializations thereof)
        case Type::InjectedClassName:
        case Type::Enum:
        {
            return makeTypeInfo<TagTypeInfo>(
                type->getAsTagDecl(), quals);
        }
        // typedef/alias type
        case Type::Typedef:
        {
            auto* T = cast<TypedefType>(type);
            return makeTypeInfo<TagTypeInfo>(
                T->getDecl(), quals);
        }
        case Type::TemplateTypeParm:
        {
            auto* T = cast<TemplateTypeParmType>(type);
            auto I = std::make_unique<BuiltinTypeInfo>();
            if(auto* D = T->getDecl())
            {
                // special case for implicit template parameters
                // resulting from abbreviated function templates
                if(D->isImplicit())
                    I->Name = "auto";
                else if(auto* II = D->getIdentifier())
                    I->Name = II->getName();
            }
            I->CVQualifiers = convertToQualifierKind(quals);
            return I;
        }
        case Type::SubstTemplateTypeParm:
        {
            auto* T = cast<SubstTemplateTypeParmType>(type);
            return buildTypeInfo(
                T->getReplacementType(), quals);
        }
        case Type::SubstTemplateTypeParmPack:
        {
            auto* T = cast<SubstTemplateTypeParmPackType>(type);
            auto I = std::make_unique<PackTypeInfo>();
            I->PatternType = makeTypeInfo<BuiltinTypeInfo>(
                T->getIdentifier(), quals);
            return I;
        }
        // builtin/unhandled type
        default:
        {
            auto I = std::make_unique<BuiltinTypeInfo>();
            I->Name = getTypeAsString(
                qt.withoutLocalFastQualifiers());
            I->CVQualifiers = convertToQualifierKind(quals);
            return I;
        }
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
                else if constexpr(kind == Decl::ClassScopeFunctionSpecialization)
                {
                    // ClassScopeFunctionSpecializationDecls only exist within the lexical
                    // definition of a ClassTemplateDecl or ClassTemplatePartialSpecializationDecl.
                    // they are never created during instantiation -- not even during the instantiation
                    // of a class template with a member class template containing such a declaration.
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

                    for(auto* R : FD->redecls())
                    {
                        if(MemberSpecializationInfo* MSI =
                            R->getMemberSpecializationInfo())
                        {
                            if(MSI->isExplicitSpecialization())
                                FD = R;
                            else
                                FD = cast<FunctionDecl>(
                                    MSI->getInstantiatedFrom());
                            break;
                        }
                        else if(R->getTemplateSpecializationKind() ==
                            TSK_ImplicitInstantiation)
                        {
                            if(auto* FTD = FD->getPrimaryTemplate())
                            {
                                FTD = cast<FunctionTemplateDecl>(
                                    getInstantiatedFrom(FTD));
                                FD = FTD->getTemplatedDecl();
                            }
                            break;
                        }
                    }

                    if(FunctionDecl* DD = FD->getDefinition())
                        FD = DD;

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
                R->Type = buildTypeInfo(
                    P->getType());
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
                    Decl* D = getInstantiatedFrom(TD);
                    extractSymbolID(D, R->Template);
                    getOrBuildInfo(D);
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
        const ClassScopeFunctionSpecializationDecl* spec)
    {
        // if(! spec->hasExplicitTemplateArgs())
        //     return;
        // KRYSTIAN NOTE: we have no way to get the ID of the primary template;
        // it is unknown what function template this will be an explicit
        // specialization of until the enclosing class template is instantiated.
        // this also means that we can only extract the explicit template arguments.
        // in the future, we could use name lookup to find matching declarations
        if(auto* args_written = spec->getTemplateArgsAsWritten())
        {
            auto args = args_written->arguments();
            buildTemplateArgs(I.Args, std::views::transform(
                args, [](auto& x) -> auto& { return x.getArgument(); }));
        }
    }

    void
    parseTemplateParams(
        TemplateInfo& I,
        const Decl* D)
    {
        if(const TemplateParameterList* ParamList =
            D->getDescribedTemplateParams())
        {
            for(const NamedDecl* ND : *ParamList)
            {
                I.Params.emplace_back(
                    buildTemplateParam(ND));
            }
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

    void
    parseEnumerators(
        EnumInfo& I,
        const EnumDecl* D)
    {
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
    }

    //------------------------------------------------

    // This also sets IsFileInRootDir
    bool
    shouldExtract(
        const Decl* D)
    {
        namespace path = llvm::sys::path;

        // skip system header
        if(! forceExtract_ && source_.isInSystemHeader(D->getLocation()))
            return false;

        const PresumedLoc loc =
            source_.getPresumedLoc(D->getBeginLoc());

        auto [it, inserted] = fileFilter_.emplace(
            loc.getIncludeLoc().getRawEncoding(),
            FileFilter());

        FileFilter& ff = it->second;
        file_ = files::makePosixStyle(loc.getFilename());

        // file has not been previously visited
        if(inserted)
            ff.include = config_.shouldExtractFromFile(file_, ff.prefix);

        // don't extract if the declaration is in a file
        // that should not be visited
        if(! forceExtract_ && ! ff.include)
            return false;
        // VFALCO we could assert that the prefix
        //        matches and just lop off the
        //        first ff.prefix.size() characters.
        path::replace_path_prefix(file_, ff.prefix, "");

        // KRYSTIAN FIXME: once set, this never gets reset
        isFileInRootDir_ = true;

        return true;
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
                    buildTypeInfo(B.getType()),
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

        parseEnumerators(I, D);

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

        for(const ParmVarDecl* P : D->parameters())
        {
            I.Params.emplace_back(
                buildTypeInfo(P->getOriginalType()),
                P->getNameAsString(),
                getSourceCode(P->getDefaultArgRange()));
        }

        I.ReturnType = buildTypeInfo(
            D->getReturnType());

        if(auto* ftsi = D->getTemplateSpecializationInfo())
        {
            if(! I.Template)
                I.Template = std::make_unique<TemplateInfo>();
            parseTemplateArgs(*I.Template, ftsi);
        }

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
};

//------------------------------------------------

bool
ASTVisitor::
traverse(NamespaceDecl* D)
{
    if(! shouldExtract(D))
        return true;
    if(! config_->includeAnonymous &&
        D->isAnonymousNamespace())
        return true;

    SymbolID id;
    if(! extractSymbolID(D, id))
        return true;
    auto [I, created] = getOrCreateInfo<NamespaceInfo>(id);

    buildNamespace(I, created, D);
    return traverseContext(D);
}

bool
ASTVisitor::
traverse(CXXRecordDecl* D,
    AccessSpecifier A,
    std::unique_ptr<TemplateInfo>&& Template = nullptr)
{
    if(! shouldExtract(D))
        return true;

    MRDOX_ASSERT(! D->getDeclContext()->isRecord() ||
        A != AccessSpecifier::AS_none);

    SymbolID id;
    if(! extractSymbolID(D, id))
        return false;
    auto [I, created] = getOrCreateInfo<RecordInfo>(id);
    I.Access = convertToAccessKind(A);
    I.Template = std::move(Template);

    buildRecord(I, created, D);
    return traverseContext(D);
}

bool
ASTVisitor::
traverse(EnumDecl* D,
    AccessSpecifier A)
{
    if(! shouldExtract(D))
        return true;

    MRDOX_ASSERT(! D->getDeclContext()->isRecord() ||
        A != AccessSpecifier::AS_none);

    SymbolID id;
    if(! extractSymbolID(D, id))
        return false;
    auto [I, created] = getOrCreateInfo<EnumInfo>(id);
    I.Access = convertToAccessKind(A);

    buildEnum(I, created, D);
    return true;
}

bool
ASTVisitor::
traverse(TypedefDecl* D,
    AccessSpecifier A)
{
    if(! shouldExtract(D))
        return true;

    MRDOX_ASSERT(! D->getDeclContext()->isRecord() ||
        A != AccessSpecifier::AS_none);

    SymbolID id;
    if(! extractSymbolID(D, id))
        return false;
    auto [I, created] = getOrCreateInfo<TypedefInfo>(id);
    I.Access = convertToAccessKind(A);

    buildTypedef(I, created, D);
    return true;
}

bool
ASTVisitor::
traverse(TypeAliasDecl* D,
    AccessSpecifier A,
    std::unique_ptr<TemplateInfo>&& Template = nullptr)
{
    if(! shouldExtract(D))
        return true;

    MRDOX_ASSERT(! D->getDeclContext()->isRecord() ||
        A != AccessSpecifier::AS_none);

    SymbolID id;
    if(! extractSymbolID(D, id))
        return false;
    auto [I, created] = getOrCreateInfo<TypedefInfo>(id);
    I.Access = convertToAccessKind(A);
    I.IsUsing = true;
    I.Template = std::move(Template);

    buildTypedef(I, created, D);
    return true;
}

bool
ASTVisitor::
traverse(VarDecl* D,
    AccessSpecifier A,
    std::unique_ptr<TemplateInfo>&& Template = nullptr)
{
    if(! shouldExtract(D))
        return true;

    MRDOX_ASSERT(! D->getDeclContext()->isRecord() ||
        A != AccessSpecifier::AS_none);

    SymbolID id;
    if(! extractSymbolID(D, id))
        return false;
    auto [I, created] = getOrCreateInfo<VariableInfo>(id);
    I.Access = convertToAccessKind(A);
    I.Template = std::move(Template);

    buildVariable(I, created, D);
    return true;
}

bool
ASTVisitor::
traverse(FieldDecl* D,
    AccessSpecifier A)
{
    if(! shouldExtract(D))
        return true;

    MRDOX_ASSERT(D->getDeclContext()->isRecord());
    MRDOX_ASSERT(A != AccessSpecifier::AS_none);

    SymbolID id;
    if(! extractSymbolID(D, id))
        return false;
    auto [I, created] = getOrCreateInfo<FieldInfo>(id);
    I.Access = convertToAccessKind(A);

    buildField(I, created, D);
    return true;
}

bool
ASTVisitor::
traverse(FunctionDecl* D,
    AccessSpecifier A,
    std::unique_ptr<TemplateInfo>&& Template = nullptr)
{
    if(! shouldExtract(D))
        return true;

    MRDOX_ASSERT(! D->getDeclContext()->isRecord());
    MRDOX_ASSERT(A == AccessSpecifier::AS_none);

    SymbolID id;
    if(! extractSymbolID(D, id))
        return false;
    auto [I, created] = getOrCreateInfo<FunctionInfo>(id);
    I.Access = convertToAccessKind(A);
    I.Template = std::move(Template);

    buildFunction(I, created, D);
    return true;
}

bool
ASTVisitor::
traverse(CXXMethodDecl* D,
    AccessSpecifier A,
    std::unique_ptr<TemplateInfo>&& Template = nullptr)
{
    if(! shouldExtract(D))
        return true;

    MRDOX_ASSERT(D->getDeclContext()->isRecord());
    MRDOX_ASSERT(A != AccessSpecifier::AS_none);

    SymbolID id;
    if(! extractSymbolID(D, id))
        return false;
    auto [I, created] = getOrCreateInfo<FunctionInfo>(id);
    I.Access = convertToAccessKind(A);
    I.Template = std::move(Template);

    buildFunction(I, created, D);
    return true;
}

bool
ASTVisitor::
traverse(CXXConstructorDecl* D,
    AccessSpecifier A,
    std::unique_ptr<TemplateInfo>&& Template = nullptr)
{
    if(! shouldExtract(D))
        return true;

    MRDOX_ASSERT(D->getDeclContext()->isRecord());
    MRDOX_ASSERT(A != AccessSpecifier::AS_none);

    SymbolID id;
    if(! extractSymbolID(D, id))
        return false;
    auto [I, created] = getOrCreateInfo<FunctionInfo>(id);
    I.Access = convertToAccessKind(A);
    I.Template = std::move(Template);
    I.Class = FunctionClass::Constructor;

    buildFunction(I, created, D);
    return true;
}

bool
ASTVisitor::
traverse(CXXConversionDecl* D,
    AccessSpecifier A,
    std::unique_ptr<TemplateInfo>&& Template = nullptr)
{
    if(! shouldExtract(D))
        return true;

    MRDOX_ASSERT(D->getDeclContext()->isRecord());
    MRDOX_ASSERT(A != AccessSpecifier::AS_none);

    SymbolID id;
    if(! extractSymbolID(D, id))
        return false;
    auto [I, created] = getOrCreateInfo<FunctionInfo>(id);
    I.Access = convertToAccessKind(A);
    I.Template = std::move(Template);
    I.Class = FunctionClass::Conversion;

    buildFunction(I, created, D);
    return true;
}

bool
ASTVisitor::
traverse(CXXDeductionGuideDecl* D,
    AccessSpecifier A,
    std::unique_ptr<TemplateInfo>&& Template = nullptr)
{
    if(! shouldExtract(D))
        return true;

    SymbolID id;
    if(! extractSymbolID(D, id))
        return false;
    auto [I, created] = getOrCreateInfo<FunctionInfo>(id);
    I.Access = convertToAccessKind(A);
    I.Template = std::move(Template);
    I.Class = FunctionClass::Deduction;

    buildFunction(I, created, D);
    return true;
}

bool
ASTVisitor::
traverse(CXXDestructorDecl* D,
    AccessSpecifier A)
{
    if(! shouldExtract(D))
        return true;

    MRDOX_ASSERT(D->getDeclContext()->isRecord());
    MRDOX_ASSERT(A != AccessSpecifier::AS_none);

    SymbolID id;
    if(! extractSymbolID(D, id))
        return false;
    auto [I, created] = getOrCreateInfo<FunctionInfo>(id);
    I.Access = convertToAccessKind(A);
    I.Class = FunctionClass::Destructor;

    buildFunction(I, created, D);
    return true;
}

bool
ASTVisitor::
traverse(FriendDecl* D)
{
    buildFriend(D);
    return true;
}

//------------------------------------------------

bool
ASTVisitor::
traverse(ClassTemplateDecl* D,
    AccessSpecifier A)
{
    CXXRecordDecl* RD = D->getTemplatedDecl();

    if(! shouldExtract(RD))
        return true;

    auto Template = std::make_unique<TemplateInfo>();
    parseTemplateParams(*Template, RD);

    return traverse(RD, A, std::move(Template));
}

bool
ASTVisitor::
traverse(ClassTemplateSpecializationDecl* D)
{
    CXXRecordDecl* RD = D;
    if(! shouldExtract(RD))
        return true;

    auto Template = std::make_unique<TemplateInfo>();
    parseTemplateParams(*Template, RD);
    parseTemplateArgs(*Template, D);

    // determine the access from the primary template
    return traverse(RD,
        D->getSpecializedTemplate()->getAccessUnsafe(),
        std::move(Template));
}

bool
ASTVisitor::
traverse(VarTemplateDecl* D,
    AccessSpecifier A)
{
    VarDecl* VD = D->getTemplatedDecl();
    if(! shouldExtract(VD))
        return true;

    auto Template = std::make_unique<TemplateInfo>();
    parseTemplateParams(*Template, VD);

    return traverse(VD, A, std::move(Template));
}

bool
ASTVisitor::
traverse(VarTemplateSpecializationDecl* D)
{
    VarDecl* VD = D;
    if(! shouldExtract(VD))
        return true;

    auto Template = std::make_unique<TemplateInfo>();
    parseTemplateParams(*Template, VD);
    parseTemplateArgs(*Template, D);

    return traverse(VD,
        D->getSpecializedTemplate()->getAccessUnsafe(),
        std::move(Template));
}

bool
ASTVisitor::
traverse(FunctionTemplateDecl* D,
    AccessSpecifier A)
{
    FunctionDecl* FD = D->getTemplatedDecl();

    // check whether to extract using the templated declaration.
    // this is done because the template-head may be implicit
    // (e.g. for an abbreviated function template with no template-head)
    if(! shouldExtract(FD))
        return true;
    auto Template = std::make_unique<TemplateInfo>();
    parseTemplateParams(*Template, FD);

    // traverse the templated declaration according to its kind
    return traverseDecl(FD, std::move(Template));
}

bool
ASTVisitor::
traverse(ClassScopeFunctionSpecializationDecl* D)
{
    if(! shouldExtract(D))
        return true;

    /* For class scope explicit specializations of member function templates which
       are members of class templates, it is impossible to know what the
       primary template is until the enclosing class template is instantiated.
       while such declarations are valid C++ (see CWG 727 and [temp.expl.spec] p3),
       GCC does not consider them to be valid. Consequently, we do not extract the SymbolID
       of the primary template. In the future, we could take a best-effort approach to find
       the primary template, but this is only possible when none of the candidates are dependent
       upon a template parameter of the enclosing class template.
    */
    auto Template = std::make_unique<TemplateInfo>();
    parseTemplateArgs(*Template, D);

    CXXMethodDecl* MD = D->getSpecialization();

    // since the templated CXXMethodDecl may be a constructor
    // or conversion function, call TraverseDecl to ensure that
    // we call traverse for the dynamic type of the CXXMethodDecl
    return traverseDecl(MD, std::move(Template));
}

bool
ASTVisitor::
traverse(TypeAliasTemplateDecl* D,
    AccessSpecifier A)
{
    TypeAliasDecl* AD = D->getTemplatedDecl();
    if(! shouldExtract(AD))
        return true;

    auto Template = std::make_unique<TemplateInfo>();
    parseTemplateParams(*Template, AD);

    return traverse(AD, A, std::move(Template));
}

template<typename... Args>
auto
ASTVisitor::
traverse(Args&&...)
{
    // no matching Traverse overload found
    MRDOX_UNREACHABLE();
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

    AccessSpecifier access =
        D->getAccessUnsafe();

    switch(D->getKind())
    {
    case Decl::Namespace:
        traverse(static_cast<
            NamespaceDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::CXXRecord:
        traverse(static_cast<
            CXXRecordDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::CXXMethod:
        traverse(static_cast<
            CXXMethodDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::CXXConstructor:
        traverse(static_cast<
            CXXConstructorDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::CXXConversion:
        traverse(static_cast<
            CXXConversionDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::CXXDestructor:
        traverse(static_cast<
            CXXDestructorDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::CXXDeductionGuide:
        traverse(static_cast<
            CXXDeductionGuideDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::Function:
        traverse(static_cast<
            FunctionDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::Friend:
        traverse(static_cast<
            FriendDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::TypeAlias:
        traverse(static_cast<
            TypeAliasDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::Typedef:
        traverse(static_cast<
            TypedefDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::Enum:
        traverse(static_cast<
            EnumDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::Field:
        traverse(static_cast<
            FieldDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::Var:
        traverse(static_cast<
            VarDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::ClassTemplate:
        traverse(static_cast<
            ClassTemplateDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::ClassTemplatePartialSpecialization:
    case Decl::ClassTemplateSpecialization:
        traverse(static_cast<
            ClassTemplateSpecializationDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::VarTemplate:
        traverse(static_cast<
            VarTemplateDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::VarTemplatePartialSpecialization:
    case Decl::VarTemplateSpecialization:
        traverse(static_cast<
            VarTemplateSpecializationDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::FunctionTemplate:
        traverse(static_cast<
            FunctionTemplateDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    case Decl::ClassScopeFunctionSpecialization:
        traverse(static_cast<
            ClassScopeFunctionSpecializationDecl*>(D),
            std::forward<Args>(args)...);
        break;
    case Decl::TypeAliasTemplate:
        traverse(static_cast<
            TypeAliasTemplateDecl*>(D),
            access,
            std::forward<Args>(args)...);
        break;
    default:
        // for declarations we don't explicitly handle, traverse the children
        // if it has any (e.g. LinkageSpecDecl, ExportDecl, ExternCContextDecl).
        if(auto* DC = dyn_cast<DeclContext>(D))
            traverseContext(DC);
        break;
    }

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
        visitor.traverseContext(
            Context.getTranslationUnitDecl());

        // dumpDeclTree(Context.getTranslationUnitDecl());

        // convert results to bitcode
        for(auto& info : visitor.results())
            insertBitcode(ex_, writeBitcode(*info));

        // VFALCO If we returned from the function early
        // then this line won't execute, which means we
        // will miss error and warnings emitted before
        // the return.
        ex_.report(std::move(diags));
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
    shouldSkipFunctionBody(Decl* D) override
    {
        return true;
    }

    bool
    HandleTopLevelDecl(DeclGroupRef DG) override
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

    void HandleInlineFunctionDefinition(FunctionDecl* D) override { }
    void HandleTagDeclDefinition(TagDecl* D) override { }
    void HandleTagDeclRequiredDefinition(const TagDecl* D) override { }
    void HandleInterestingDecl(DeclGroupRef DG) override { }
    void CompleteTentativeDefinition(VarDecl* D) override { }
    void CompleteExternalDeclaration(VarDecl* D) override { }
    void AssignInheritanceModel(CXXRecordDecl* D) override { }
    void HandleVTable(CXXRecordDecl* D) override { }
    void HandleImplicitImportDecl(ImportDecl* D) override { }
    void HandleTopLevelDeclInObjCContainer(DeclGroupRef DG) override { }

public:
    ASTVisitorConsumer(
        const ConfigImpl& config,
        tooling::ExecutionContext& ex,
        CompilerInstance& compiler) noexcept
        : config_(config)
        , ex_(static_cast<ExecutionContext&>(ex))
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
        tooling::ExecutionContext& ex,
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
    tooling::ExecutionContext& ex_;
    ConfigImpl const& config_;
};

//------------------------------------------------

struct ASTActionFactory :
    tooling::FrontendActionFactory
{
    ASTActionFactory(
        tooling::ExecutionContext& ex,
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
    tooling::ExecutionContext& ex_;
    ConfigImpl const& config_;
};

} // (anon)

//------------------------------------------------

std::unique_ptr<tooling::FrontendActionFactory>
makeFrontendActionFactory(
    tooling::ExecutionContext& ex,
    ConfigImpl const& config)
{
    return std::make_unique<ASTActionFactory>(ex, config);
}

} // mrdox
} // clang
