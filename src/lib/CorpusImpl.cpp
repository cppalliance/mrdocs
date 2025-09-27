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

#include "CorpusImpl.hpp"
#include <lib/AST/FrontendActionFactory.hpp>
#include <lib/AST/MissingSymbolSink.hpp>
#include <lib/AST/MrDocsFileSystem.hpp>
#include <lib/Metadata/Finalizers/BaseMembersFinalizer.hpp>
#include <lib/Metadata/Finalizers/DerivedFinalizer.hpp>
#include <lib/Metadata/Finalizers/JavadocFinalizer.hpp>
#include <lib/Metadata/Finalizers/NamespacesFinalizer.hpp>
#include <lib/Metadata/Finalizers/OverloadsFinalizer.hpp>
#include <lib/Metadata/Finalizers/SortMembersFinalizer.hpp>
#include <lib/Support/Chrono.hpp>
#include <lib/Support/Report.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Support/Algorithm.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/ThreadPool.hpp>
#include <chrono>

namespace mrdocs {

auto
CorpusImpl::
begin() const noexcept ->
    iterator
{
    if(info_.empty())
        return end();
    // KRYSTIAN NOTE: this is far from ideal, but i'm not sure
    // to what extent implementation detail should be hidden.
    return iterator(this, info_.begin()->get(),
        [](Corpus const* corpus, Symbol const* val) -> Symbol const*
        {
            MRDOCS_ASSERT(val);
            CorpusImpl const* impl =
                static_cast<CorpusImpl const*>(corpus);
            auto it = impl->info_.find(val->id);
            if(++it == impl->info_.end())
                return nullptr;
            return it->get();
        });
}

auto
CorpusImpl::
end() const noexcept ->
    iterator
{
    return iterator();
}

Symbol*
CorpusImpl::
find(
    SymbolID const& id) noexcept
{
    auto const it = info_.find(id);
    if (it != info_.end())
    {
        return it->get();
    }
    return nullptr;
}

Symbol const*
CorpusImpl::
find(SymbolID const& id) const noexcept
{
    auto const it = info_.find(id);
    if (it != info_.end())
    {
        return it->get();
    }
    return nullptr;
}

namespace {
bool
isTransparent(Symbol const& info)
{
    return visit(info, []<typename InfoTy>(
        InfoTy const& I) -> bool
    {
        if constexpr (InfoTy::isNamespace())
        {
            return I.IsInline;
        }
        if constexpr (InfoTy::isEnum())
        {
            return !I.Scoped;
        }
        return false;
    });
}

SymbolID
findFirstParentInfo(
    SymbolSet const& info,
    SymbolID const& contextID)
{
    SymbolID currentID = contextID;

    while (true)
    {
        auto const contextIt = info.find(currentID);
        MRDOCS_CHECK_OR(contextIt != info.end(), SymbolID::invalid);
        auto& contextUniquePtr = *contextIt;
        MRDOCS_CHECK_OR(contextUniquePtr, SymbolID::invalid);
        auto& context = *contextUniquePtr;
        bool const isParent = visit(context, []<typename InfoTy>(
            InfoTy const& I) -> bool
        {
            return SymbolParent<InfoTy>;
        });
        if (isParent)
        {
            return context.id;
        }
        currentID = context.Parent;
    }
}

bool
qualifiedNameCompare(
    Polymorphic<Name> const& lhs0,
    Polymorphic<Name> const& rhs0,
    Symbol const& context,
    CorpusImpl const& corpus)
{
    MRDOCS_ASSERT(!lhs0.valueless_after_move());
    MRDOCS_ASSERT(!rhs0.valueless_after_move());
    // Compare each component of the qualified name
    Name const* lhs = &*lhs0;
    Name const* rhs = &*rhs0;
    while (lhs && rhs)
    {
        if (lhs->Identifier != rhs->Identifier)
        {
            return false;
        }
        lhs = lhs->Prefix ? &**lhs->Prefix : nullptr;
        rhs = rhs->Prefix ? &**rhs->Prefix : nullptr;
    }
    // We consumed all components of both names
    if (!lhs && !rhs)
    {
        return true;
    }
    // One name has more components than the other:
    // these component should match the names from
    // the context. When we doesn't match the context
    // we try again with the parent context.
    Name const* curName0 = lhs ? lhs : rhs;
    Symbol const* curContext0 = &context;
    Name const* curName = curName0;
    Symbol const* curContext = curContext0;
    while (curName && curContext)
    {
        if (curName->Identifier != curContext->Name)
        {
            // The name doesn't match the context name
            // Try again, starting from the parent
            // context.
            curName = curName0;
            curContext0 = curContext0->Parent ? corpus.find(curContext->Parent) : nullptr;
            // No parent context to try: return false
            if (!curContext0)
            {
                return false;
            }
            curContext = curContext0;
            continue;
        }
        // Names matches, match next component
        curName = curName->Prefix ? &**curName->Prefix : nullptr;
        curContext = curContext->Parent ? corpus.find(curContext->Parent) : nullptr;
    }
    // We should have consumed all components of the name with the context
    return !curName;
}

template <bool isInner>
bool
isDecayedEqualImpl(
    Optional<Polymorphic<Type>> const& lhs,
    Optional<Polymorphic<Type>> const& rhs,
    Symbol const& context,
    CorpusImpl const& corpus);

// Check if two types are equal after decay
//
// The isInner template parameter indicates if
// we are comparing inner types (e.g., pointee types)
// or root types (e.g., function parameter types) because
// the rules are slightly different depending
// on the level of the type specifiers.
//
template <bool isInner>
bool
isDecayedEqualImpl(
    Polymorphic<Type> const& lhs,
    Polymorphic<Type> const& rhs,
    Symbol const& context,
    CorpusImpl const& corpus)
{
    // Polymorphic
    MRDOCS_ASSERT(!lhs.valueless_after_move());
    MRDOCS_ASSERT(!rhs.valueless_after_move());
    // Type
    bool const decayToPointer = !isInner && (lhs->isArray() || rhs->isArray());
    if (!decayToPointer)
    {
        MRDOCS_CHECK_OR(lhs->Kind == rhs->Kind, false);
    }
    else
    {
        // in root types, arrays are decayed to pointers
        MRDOCS_CHECK_OR(lhs->isArray() || lhs->isPointer(), false);
        MRDOCS_CHECK_OR(rhs->isArray() || rhs->isPointer(), false);
    }
    MRDOCS_CHECK_OR(lhs->IsPackExpansion == rhs->IsPackExpansion, false);
    if constexpr (isInner)
    {
        // const and volatile are ignored from root types
        // in function parameters
        MRDOCS_CHECK_OR(lhs->IsConst == rhs->IsConst, false);
        MRDOCS_CHECK_OR(lhs->IsVolatile == rhs->IsVolatile, false);
    }
    MRDOCS_CHECK_OR(lhs->Constraints == rhs->Constraints, false);
    switch (lhs->Kind)
    {
    // Types that never decay are compared directly, but we
    // only compare the fields of the type, without reevaluating
    // the fields of Type.
    case TypeKind::Named:
    {
        return
            qualifiedNameCompare(
                lhs->asNamed().Name,
                rhs->asNamed().Name,
                context, corpus);
    }
    case TypeKind::Decltype:
    {
        return lhs->asDecltype().Operand ==
               rhs->asDecltype().Operand;
    }
    case TypeKind::Auto:
    {
        auto const& lhsAuto = lhs->asAuto();
        auto const& rhsAuto = rhs->asAuto();
        return lhsAuto.Keyword == rhsAuto.Keyword &&
               lhsAuto.Constraint == rhsAuto.Constraint;
    }
    case TypeKind::LValueReference:
    {
        return
            isDecayedEqualImpl<true>(
                lhs->asLValueReference().PointeeType,
                rhs->asLValueReference().PointeeType,
                context, corpus);
    }
    case TypeKind::RValueReference:
    {
        return
            isDecayedEqualImpl<true>(
                dynamic_cast<RValueReferenceType const&>(*lhs).PointeeType,
                dynamic_cast<RValueReferenceType const&>(*rhs).PointeeType,
                context, corpus);
    }
    case TypeKind::MemberPointer:
    {
        auto const& lhsMP = dynamic_cast<MemberPointerType const&>(*lhs);
        auto const& rhsMP = dynamic_cast<MemberPointerType const&>(*rhs);
        return
            isDecayedEqualImpl<true>(lhsMP.PointeeType, rhsMP.PointeeType, context, corpus) &&
            isDecayedEqualImpl<true>(lhsMP.ParentType, rhsMP.ParentType, context, corpus);
    }
    case TypeKind::Function:
    {
        auto const& lhsF = dynamic_cast<FunctionType const&>(*lhs);
        auto const& rhsF = dynamic_cast<FunctionType const&>(*rhs);
        MRDOCS_CHECK_OR(lhsF.RefQualifier == rhsF.RefQualifier, false);
        MRDOCS_CHECK_OR(lhsF.ExceptionSpec == rhsF.ExceptionSpec, false);
        MRDOCS_CHECK_OR(lhsF.IsVariadic == rhsF.IsVariadic, false);
        MRDOCS_CHECK_OR(isDecayedEqualImpl<true>(lhsF.ReturnType, rhsF.ReturnType, context, corpus), false);
        MRDOCS_CHECK_OR(lhsF.ParamTypes.size() == rhsF.ParamTypes.size(), false);
        for (std::size_t i = 0; i < lhsF.ParamTypes.size(); ++i)
        {
            MRDOCS_CHECK_OR(isDecayedEqualImpl<false>(lhsF.ParamTypes[i], rhsF.ParamTypes[i], context, corpus), false);
        }
        return true;
    }
    // Types that should decay
    case TypeKind::Pointer:
    case TypeKind::Array:
    {
        auto const I1 = innerType(*lhs);
        auto const I2 = innerType(*rhs);
        // Both inner types must be present or absent, otherwise not equal
        MRDOCS_CHECK_OR(static_cast<bool>(I1) == static_cast<bool>(I2), false);
        // Both inner types are absent: they are equal
        MRDOCS_CHECK_OR(static_cast<bool>(I1) && static_cast<bool>(I2), true);
        // Both inner types are present: compare them internally
        return isDecayedEqualImpl<true>(*I1, *I2, context, corpus);
    }
    default:
        MRDOCS_UNREACHABLE();
    }
    return true;
}

template <bool isInner>
bool
isDecayedEqualImpl(
    Optional<Polymorphic<Type>> const& lhs,
    Optional<Polymorphic<Type>> const& rhs,
    Symbol const& context,
    CorpusImpl const& corpus)
{
    MRDOCS_CHECK_OR(static_cast<bool>(lhs) == static_cast<bool>(rhs), false);
    MRDOCS_CHECK_OR(static_cast<bool>(lhs) && static_cast<bool>(rhs), true);
    return isDecayedEqualImpl<isInner>(*lhs, *rhs, context, corpus);
}

/* Compare two types for equality for the purposes of
   overload resolution.
 */
bool
isDecayedEqual(
    Polymorphic<Type> const& lhs,
    Polymorphic<Type> const& rhs,
    Symbol const& context,
    CorpusImpl const& corpus)
{
    return isDecayedEqualImpl<false>(lhs, rhs, context, corpus);
}

bool
isDecayedEqual(
    Polymorphic<TArg> const& lhs,
    Polymorphic<TArg> const& rhs,
    Symbol const& context,
    CorpusImpl const& corpus)
{
    if (lhs->Kind != rhs->Kind)
    {
        return false;
    }
    if (lhs->isType())
    {
        return isDecayedEqualImpl<true>(static_cast<const TypeTArg &>(*lhs).Type,
                                        static_cast<const TypeTArg &>(*rhs).Type,
                                        context, corpus);
    }
    if (lhs->isConstant())
    {
        return trim(static_cast<ConstantTArg const&>(*lhs).Value.Written) ==
               trim(static_cast<ConstantTArg const&>(*rhs).Value.Written);
    }
    return false;
}
}

Expected<Symbol const&>
CorpusImpl::
lookup(SymbolID const& context, std::string_view const name) const
{
    return lookupImpl(*this, context, name);
}

Expected<Symbol const&>
CorpusImpl::
lookup(SymbolID const& context, std::string_view name)
{
    return lookupImpl(*this, context, name);
}

template <class Self>
Expected<Symbol const&>
CorpusImpl::
lookupImpl(Self&& self, SymbolID const& contextId0, std::string_view name)
{
    report::trace("Looking up '{}'", name);
    if (name.starts_with("::"))
    {
        return lookupImpl(self, SymbolID::global, name.substr(2));
    }
    // Skip contexts that can't have members
    SymbolID const contextId = findFirstParentInfo(self.info_, contextId0);
    MRDOCS_CHECK(contextId != SymbolID::invalid, formatError("Failed to find '{}'", contextId0));
    report::trace("    Context: '{}'", contextId);
    if (auto [info, found] = self.lookupCacheGet(contextId, name);
        found)
    {
        if (!info)
        {
            return Unexpected(formatError(
                "Failed to find '{}' from context '{}'",
                name,
                self.Corpus::qualifiedName(*self.find(contextId))));
        }
        return *info;
    }
    auto const expRef = parse<ParsedRef>(name);
    if (!expRef)
    {
        return Unexpected(formatError("Failed to parse '{}'\n     {}", name, expRef.error().reason()));
    }
    ParsedRef const& ref = *std::move(expRef);
    Symbol const* res = lookupImpl(self, contextId, ref, name, false);
    if (!res)
    {
        auto const contextPtr = self.find(contextId);
        if (!contextPtr)
        {
            return Unexpected(formatError("Failed to find '{}'", contextId));
        }
        return Unexpected(formatError(
            "Failed to find '{}' from context '{}'",
            name,
            self.Corpus::qualifiedName(*contextPtr)));
    }
    return *res;
}

template <class Self>
Symbol const*
CorpusImpl::
lookupImpl(
    Self&& self,
    SymbolID const& contextId,
    ParsedRef const& ref,
    std::string_view name,
    bool const cache)
{
    report::trace("Looking up parsed '{}'", name);
    if (cache)
    {
        if (auto [info, found] = self.lookupCacheGet(contextId, name); found)
        {
            return info;
        }
    }

    Symbol const* contextPtr = self.find(contextId);
    MRDOCS_CHECK_OR(contextPtr, nullptr);
    report::trace("    Context: '{}'", contextPtr->Name);

    // Check the current contextId
    Symbol const* curContext = contextPtr;
    for (std::size_t i = 0; i < ref.Components.size(); ++i)
    {
        ParsedRefComponent const& component = ref.Components[i];
        bool const isLast = i == ref.Components.size() - 1;
        curContext = self.lookupImpl(curContext->id, component, ref, isLast);
        if (!curContext)
        {
            break;
        }
    }
    if (curContext)
    {
        self.lookupCacheSet(contextId, name, curContext);
        return curContext;
    }

    // Fallback to parent contextId
    Symbol const& contextInfo = *contextPtr;
    Symbol const* res = lookupImpl(self, contextInfo.Parent, ref, name, true);
    self.lookupCacheSet(contextId, name, res);
    return res;
}

Symbol const*
CorpusImpl::
lookupImpl(
    SymbolID const& contextId,
    ParsedRefComponent const& component,
    ParsedRef const& ref,
    bool const checkParameters) const
{
    report::trace("Looking up component '{}'", component.Name);
    // 1. Find context
    Symbol const* contextPtr = this->find(contextId);
    MRDOCS_CHECK_OR(contextPtr, nullptr);
    report::trace("    Context: '{}'", contextPtr->Name);

    // If context is a typedef, lookup in the resolved type
    if (auto* TI = contextPtr->asTypedefPtr())
    {
        MRDOCS_CHECK_OR(TI->Type, nullptr);
        MRDOCS_ASSERT(!TI->Type.valueless_after_move());
        SymbolID resolvedSymbolID = (*TI->Type).namedSymbol();
        contextPtr = this->find(resolvedSymbolID);
        MRDOCS_CHECK_OR(contextPtr, nullptr);
    }
    Symbol const& context = *contextPtr;

    // 2. Get a list of members
    report::trace("    Finding members of context '{}'", contextPtr->Name);
    llvm::SmallVector<SymbolID, 128> memberIDs;
    auto pushAllMembersOf = [&](Symbol const& I)
    {
        visit(I, [&]<typename CInfoTy>(CInfoTy const& C) -> Symbol const*
        {
            if constexpr (SymbolParent<CInfoTy>)
            {
                for (SymbolID const& MId : allMembers(C))
                {
                    memberIDs.push_back(MId);
                }
            }
            return nullptr;
        });
    };
    pushAllMembersOf(context);
    MRDOCS_CHECK_OR(!memberIDs.empty(), nullptr);

    // If a member is overloads, we also push the specific overloads
    std::size_t const rootMembersSize = memberIDs.size();
    for (std::size_t i = 0; i < rootMembersSize; ++i)
    {
        SymbolID const& id = memberIDs[i];
        Symbol const* I = find(id);
        MRDOCS_CHECK_OR_CONTINUE(I);
        MRDOCS_CHECK_OR_CONTINUE(I->isOverloads());
        pushAllMembersOf(*I);
    }

    // 3. Find the member that matches the component
    enum class MatchLevel {
        None,
        Name,
        TemplateArgsSize,
        TemplateArgs,
        FunctionParametersSize,
        FunctionParametersSizeAndDocumented,
        FunctionParameters,
        Qualifiers,
        NoExceptDefinition,
        NoExceptKind,
        NoExceptOperand
    };
    auto const highestMatchLevel =
        !checkParameters || !ref.HasFunctionParameters ?
            MatchLevel::TemplateArgs :
            MatchLevel::Qualifiers;
    auto matchLevel = MatchLevel::None;
    Symbol const* res = nullptr;
    for (SymbolID const& memberID : memberIDs)
    {
        Symbol const* memberPtr = find(memberID);
        MRDOCS_CHECK_OR_CONTINUE(memberPtr);
        Symbol const& member = *memberPtr;
        report::trace("    Attempting to match {} '{}'", toString(member.Kind), member.Name);
        MatchLevel const memberMatchLevel =
            visit(member, [&]<typename MInfoTy>(MInfoTy const& M)
                -> MatchLevel
        {
            auto matchRes = MatchLevel::None;

            // Name match
            if constexpr (
                std::same_as<MInfoTy, FunctionSymbol> ||
                std::same_as<MInfoTy, OverloadsSymbol>)
            {
                if (component.isOperator())
                {
                    MRDOCS_CHECK_OR(M.OverloadedOperator == component.Operator, matchRes);
                }
                else if (component.isConversion())
                {
                    MRDOCS_CHECK_OR(M.Class == FunctionClass::Conversion, matchRes);
                    MRDOCS_CHECK_OR(component.ConversionType == M.ReturnType, matchRes);
                }
                else
                {
                    MRDOCS_CHECK_OR(member.Name == component.Name, matchRes);
                }
            }
            else
            {
                MRDOCS_CHECK_OR(member.Name == component.Name, matchRes);
            }
            matchRes = MatchLevel::Name;

            // Template arguments size match
            TemplateInfo const* templateInfo = [&]() -> TemplateInfo const* {
                if constexpr (requires { M.Template; })
                {
                    Optional<TemplateInfo> const& OTI = M.Template;
                    MRDOCS_CHECK_OR(OTI, nullptr);
                    TemplateInfo const& TI = *OTI;
                    return &TI;
                }
                else
                {
                    return nullptr;
                }
            }();
            if (!templateInfo)
            {
                MRDOCS_CHECK_OR(!component.HasTemplateArguments, matchRes);
            }
            else
            {
                MRDOCS_CHECK_OR(templateInfo->Args.size() == component.TemplateArguments.size(), matchRes);
            }
            matchRes = MatchLevel::TemplateArgsSize;

            if (templateInfo)
            {
                for (std::size_t i = 0; i < templateInfo->Args.size(); ++i)
                {
                    auto& lhsType = templateInfo->Args[i];
                    auto& rhsType  = component.TemplateArguments[i];
                    MRDOCS_CHECK_OR(isDecayedEqual(lhsType, rhsType, context, *this), matchRes);
                }
            }
            matchRes = MatchLevel::TemplateArgs;

            // Function parameters size match
            MRDOCS_CHECK_OR(checkParameters && ref.HasFunctionParameters, matchRes);
            MRDOCS_CHECK_OR(MInfoTy::isFunction(), matchRes);
            auto const& F = M.asFunction();
            MRDOCS_CHECK_OR(F.Params.size() == ref.FunctionParameters.size(), matchRes);
            matchRes = MatchLevel::FunctionParametersSize;

            // Function parameters size and documented match
            // This is an intermediary level because among choices that
            // doesn't exactly match the function parameters, we prefer
            // the one that is documented as the most "natural" choice.
            if (F.javadoc)
            {
                matchRes = MatchLevel::FunctionParametersSizeAndDocumented;
            }

            // Function parameters match
            MRDOCS_CHECK_OR(F.IsExplicitObjectMemberFunction == ref.IsExplicitObjectMemberFunction, matchRes);
            for (std::size_t i = 0; i < F.Params.size(); ++i)
            {
                Polymorphic<Type> const& lhsType = F.Params[i].Type;
                MRDOCS_ASSERT(!lhsType.valueless_after_move());
                auto& rhsType  = ref.FunctionParameters[i];
                MRDOCS_CHECK_OR(isDecayedEqual(lhsType, rhsType, context, *this), matchRes);
            }
            MRDOCS_CHECK_OR(F.IsVariadic == ref.IsVariadic, matchRes);
            matchRes = MatchLevel::FunctionParameters;

            // Qualifiers match
            MRDOCS_CHECK_OR(F.RefQualifier == ref.Kind, matchRes);
            MRDOCS_CHECK_OR(F.IsConst == ref.IsConst, matchRes);
            MRDOCS_CHECK_OR(F.IsVolatile == ref.IsVolatile, matchRes);
            matchRes = MatchLevel::Qualifiers;

            // Noexcept match
            MRDOCS_CHECK_OR(F.Noexcept.Implicit == ref.ExceptionSpec.Implicit, matchRes);
            matchRes = MatchLevel::NoExceptDefinition;
            MRDOCS_CHECK_OR(F.Noexcept.Kind == ref.ExceptionSpec.Kind, matchRes);
            matchRes = MatchLevel::NoExceptKind;
            MRDOCS_CHECK_OR(F.Noexcept.Operand == ref.ExceptionSpec.Operand, matchRes);
            matchRes = MatchLevel::NoExceptOperand;
            return matchRes;
        });
        if (memberMatchLevel > matchLevel)
        {
            res = &member;
            matchLevel = memberMatchLevel;
            // Early exit if the match level is the highest possible
            // for the component and the parsed reference
            if (matchLevel >= highestMatchLevel)
            {
                break;
            }
        }
    }

    // If we found a match, return it
    if (matchLevel != MatchLevel::None)
    {
        return res;
    }

    // Else, fallback to transparent contexts
    report::trace("    Looking up in transparent contexts");
    for (SymbolID const& memberID : memberIDs)
    {
        Symbol const* memberPtr = find(memberID);
        MRDOCS_CHECK_OR_CONTINUE(memberPtr);
        Symbol const& member = *memberPtr;
        MRDOCS_CHECK_OR_CONTINUE(isTransparent(member));
        if (Symbol const* r = lookupImpl(member.id, component, ref, checkParameters))
        {
            return r;
        }
    }

    // If we didn't find a match, return nullptr
    return nullptr;
}

std::pair<Symbol const*, bool>
CorpusImpl::
lookupCacheGet(
    SymbolID const& context,
    std::string_view name) const
{
    auto contextTableIt = lookupCache_.find(context);
    if (contextTableIt == lookupCache_.end())
    {
        return { nullptr, false };
    }
    auto const& contextTable = contextTableIt->second;
    auto const lookupResIt = contextTable.find(name);
    if (lookupResIt == contextTable.end())
    {
        return { nullptr, false };
    }
    return { lookupResIt->second, true };
}

void
CorpusImpl::
lookupCacheSet(
    SymbolID const& contextId,
    std::string_view name,
    Symbol const* info)
{
    lookupCache_[contextId][std::string(name)] = info;
}


//------------------------------------------------

mrdocs::Expected<std::unique_ptr<Corpus>>
CorpusImpl::
build(
    std::shared_ptr<ConfigImpl const> const& config,
    clang::tooling::CompilationDatabase const& compilations)
{
    using clock_type = std::chrono::steady_clock;
    auto start_time = clock_type::now();

    // ------------------------------------------
    // Create empty corpus
    // ------------------------------------------
    // The corpus will keep a reference to Config.
    auto corpus = std::make_unique<CorpusImpl>(config);

    // ------------------------------------------
    // Execution context
    // ------------------------------------------
    // Create an execution context to store the
    // results of the AST traversal.
    // Any new Info objects will be added to the
    // SymbolSet in the execution context.
    InfoExecutionContext context(*config);

    // ------------------------------------------
    // "Process file" task
    // ------------------------------------------
    auto const processFile = [&](std::string path) {
        // Create an `ASTActionFactory` to create multiple
        // `ASTAction`s that extract the AST for each translation unit.
        // Each CompilerInstance is used only once.
        // Per-file sink: no sharing, no races
        MissingSymbolSink sink;
        ASTActionFactory actionFactory(context, *config, sink);

        // Retry loop: grow a per-file shim and re-run
        std::size_t prevCount = 0;
        constexpr unsigned kMaxCollectSteps = 1000;
        int rc = 1;

        for (unsigned attempt = 0; attempt <= kMaxCollectSteps; ++attempt)
        {
            // Per-file (per-thread) VFS instance: safe to reuse across retries
            auto FS = createMrDocsFileSystem(*config);
            auto* FSConcrete = dynamic_cast<mrdocs::MrDocsFileSystem*>(FS.get());
            MRDOCS_ASSERT(
                FSConcrete
                && "createMrDocsFileSystem must return MrDocsFileSystem");

            // Create a Clang Tool to run the action
            auto PHCCOntainerOps = std::make_shared<clang::PCHContainerOperations>();
            clang::tooling::ClangTool
                Tool(compilations, { path }, std::move(PHCCOntainerOps), FS);
            Tool.setPrintErrorMessage(false);

            // Set the shim for missing symbols
            Tool.clearArgumentsAdjusters();
            if (sink.numSymbols())
            {
                // Build/refresh this TU’s shim and publish it in this VFS
                std::string shimContent = sink.buildShim();
                auto makeShimPathPlatformAbsolute = []()-> std::string
                {
                    llvm::SmallString<260> wd;
                    auto ec = llvm::sys::fs::current_path(wd);
                    (void) ec;
                    // "C:"
                    llvm::SmallString<16> root(llvm::sys::path::root_name(wd));
                    llvm::sys::path::append(
                        root,
                        llvm::sys::path::root_directory(wd)); // "C:/"
                    llvm::SmallString<260> p(root);
                    llvm::sys::path::append(
                        p,
                        "__mrdocs_shims",
                        "virtual_diagnostics_shim.hpp");
                    llvm::sys::path::native(p, llvm::sys::path::Style::posix);
                    return std::string(p.str());
                };

                std::string shimPath = makeShimPathPlatformAbsolute();
                FSConcrete->addVirtualFile(shimPath, shimContent);
                Tool.appendArgumentsAdjuster(
                    clang::tooling::combineAdjusters(
                        clang::tooling::getInsertArgumentAdjuster("-include"),
                        clang::tooling::getInsertArgumentAdjuster(shimPath.data())));
            }

            // Run the action
            rc = Tool.run(&actionFactory);

            // Check for errors
            std::size_t const curCount = sink.numSymbols();
            MRDOCS_ASSERT(curCount >= prevCount);
            if (rc != 0)
            {
                // Regular failure: break
                break;
            }
            if (curCount == prevCount)
            {
                // Success and no new missing symbols: done
                break;
            }
            // Failure and new symbols: reevaluate TU with new shim
            prevCount = curCount;
        }

        if (rc != 0)
        {
            formatError("Failed to run action on {}", path).Throw();
        }
    };

    // ------------------------------------------
    // Run the process file task on all files
    // ------------------------------------------
    // Traverse the AST for all translation units.
    // This operation happens on a thread pool.
    report::info("Extracting declarations");

    // Get a copy of the filename strings
    std::vector<std::string> files = compilations.getAllFiles();
    MRDOCS_CHECK(files, "Compilations database is empty");
    std::vector<Error> errors;

    // Run the action on all files in the database
    if (files.size() == 1)
    {
        try
        {
            processFile(std::move(files.front()));
        }
        catch (Exception const& ex)
        {
            errors.push_back(ex.error());
        }
    }
    else
    {
        TaskGroup taskGroup(config->threadPool());
        std::size_t index = 0;
        for (std::string& file : files)
        {
            taskGroup.async(
            [&, idx = ++index, path = std::move(file)]()
            {
                report::debug("[{}/{}] \"{}\"", idx, files.size(), path);
                processFile(path);
            });
        }
        errors = taskGroup.wait();
    }
    // Print diagnostics totals
    context.reportEnd(report::Level::info);

    // ------------------------------------------
    // Report warning and error totals
    // ------------------------------------------
    if (!errors.empty())
    {
        Error err(errors);
        if (!(*config)->ignoreFailures)
        {
            return Unexpected(err);
        }
        report::warn(
            "Warning: mapping failed because ", err);
    }

    MRDOCS_TRY(auto results, context.results());
    auto undocumented = context.undocumented();
    corpus->info_ = std::move(results);
    corpus->undocumented_ = std::move(undocumented);

    // ------------------------------------------
    // Finalize corpus
    // ------------------------------------------
    corpus->finalize();

    report::info(
        "Extracted {} declarations in {}",
        corpus->info_.size(),
        format_duration(clock_type::now() - start_time));
    if (report::getMinimumLevel() <= report::Level::info)
    {
        for (ExtractionMode m:
             { ExtractionMode::Regular,
               ExtractionMode::SeeBelow,
               ExtractionMode::ImplementationDefined,
               ExtractionMode::Dependency })
        {
            std::size_t const count = std::ranges::
                count_if(corpus->info_, [m](auto const& info) {
                return info && info->Extraction == m;
            });
            MRDOCS_CHECK_OR_CONTINUE(count);
            report::info(
                "  - {} symbols: {}",
                toString(m),
                count);
        }
    }

    return corpus;
}

void
CorpusImpl::
qualifiedName(Symbol const& I, std::string& result) const
{
    result.clear();
    if (!I.id || I.id == SymbolID::global)
    {
        return;
    }

    if (I.Parent &&
        I.Parent != SymbolID::global)
    {
        if (Symbol const* PI = find(I.Parent))
        {
            qualifiedName(*PI, result);
            result += "::";
        }
    }
    if (!I.Name.empty())
    {
        result += I.Name;
    }
    else
    {
        result += "<unnamed ";
        result += toString(I.Kind);
        result += ">";
    }
}

void
CorpusImpl::
qualifiedName(
    Symbol const& I,
    SymbolID const& context,
    std::string& result) const
{
    if (context == SymbolID::global)
    {
        qualifiedName(I, result);
        return;
    }

    result.clear();
    if (!I.id || I.id == SymbolID::global)
    {
        return;
    }

    if (I.Parent &&
        !is_one_of(I.Parent, {SymbolID::global, context}) &&
        I.id != context)
    {
        if (Symbol const* PI = find(I.Parent))
        {
            qualifiedName(*PI, context, result);
            result += "::";
        }
    }

    if (I.id == context)
    {
        return;
    }

    if (I.Parent == SymbolID::global)
    {
        result += "::";
    }
    if (!I.Name.empty())
    {
        result += I.Name;
    }
    else
    {
        result += "<unnamed ";
        result += toString(I.Kind);
        result += ">";
    }
}

void
CorpusImpl::finalize()
{
    report::info("Finalizing corpus");

    {
        report::debug("  - Finalizing namespaces");
        NamespacesFinalizer finalizer(*this);
        finalizer.build();
    }

    if (config->inheritBaseMembers != PublicSettings::BaseMemberInheritance::Never)
    {
        report::debug("  - Finalizing base members");
        BaseMembersFinalizer finalizer(*this);
        finalizer.build();
    }

    if (config->overloads)
    {
        report::debug("  - Finalizing overloads");
        OverloadsFinalizer finalizer(*this);
        finalizer.build();
    }

    // Add auto relates for member functions
    {
        report::debug("  - Finalizing auto-relates");
        DerivedFinalizer finalizer(*this);
        finalizer.build();
    }

    // Sort members: this runs unconditionally because
    // the members of some symbol types are always sorted.
    {
        report::debug("  - Finalizing sorted members");
        SortMembersFinalizer finalizer(*this);
        finalizer.build();
    }

    // Finalize javadoc
    {
        report::debug("  - Finalizing javadoc");
        JavadocFinalizer finalizer(*this);
        finalizer.build();
    }
}

} // mrdocs
