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
#include "lib/AST/FrontendActionFactory.hpp"
#include "lib/Metadata/Finalizers/BaseMembersFinalizer.hpp"
#include "lib/Metadata/Finalizers/OverloadsFinalizer.hpp"
#include "lib/Metadata/Finalizers/SortMembersFinalizer.hpp"
#include "lib/Metadata/Finalizers/JavadocFinalizer.hpp"
#include "lib/Metadata/Finalizers/NamespacesFinalizer.hpp"
#include "lib/Support/Report.hpp"
#include "lib/Support/Chrono.hpp"
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/ThreadPool.hpp>
#include <chrono>

namespace clang::mrdocs {

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
        [](Corpus const* corpus, Info const* val) ->
            Info const*
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

Info*
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

Info const*
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
isTransparent(Info const& info)
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
    InfoSet const& info,
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
            return InfoParent<InfoTy>;
        });
        if (isParent)
        {
            return context.id;
        }
        currentID = context.Parent;
    }
}

bool
isDecayedEqual(
    Polymorphic<TypeInfo> const& lhs,
    Polymorphic<TypeInfo> const& rhs)
{
    // When comparing two parameters, we need to compare
    // them as if they are decayed at the top level
    // the same way function parameters are decayed to
    // consider if they redefine an overload.
    if (lhs == rhs)
    {
        return true;
    }

    // const and volatile are ignored at the top level
    auto lhsDecay = lhs;
    lhsDecay->IsConst = false;
    lhsDecay->IsVolatile = false;
    auto rhsDecay = rhs;
    rhsDecay->IsConst = false;
    rhsDecay->IsVolatile = false;

    // Arrays decay to pointer
    if (lhsDecay->isArray())
    {
        auto& lhsAsArray = dynamic_cast<ArrayTypeInfo&>(*lhsDecay);
        PointerTypeInfo lhsAsPtr;
        lhsAsPtr.PointeeType = std::move(lhsAsArray.ElementType);
        // Copy all fields from base type
        dynamic_cast<TypeInfo&>(lhsAsPtr) = dynamic_cast<TypeInfo&>(lhsAsArray);
        lhsAsPtr.Kind = TypeKind::Pointer;
        lhsDecay = std::move(lhsAsPtr);
    }
    if (rhsDecay->isArray())
    {
        auto& rhsAsArray = dynamic_cast<ArrayTypeInfo&>(*rhsDecay);
        PointerTypeInfo rhsAsPtr;
        rhsAsPtr.PointeeType = std::move(rhsAsArray.ElementType);
        // Copy all fields from base type
        dynamic_cast<TypeInfo&>(rhsAsPtr) = dynamic_cast<TypeInfo&>(rhsAsArray);
        rhsAsPtr.Kind = TypeKind::Pointer;
        rhsDecay = std::move(rhsAsPtr);
    }

    return lhsDecay == rhsDecay;
}
}

Expected<std::reference_wrapper<Info const>>
CorpusImpl::
lookup(SymbolID const& context, std::string_view const name) const
{
    return lookupImpl(*this, context, name);
}

Expected<std::reference_wrapper<Info const>>
CorpusImpl::
lookup(SymbolID const& context, std::string_view name)
{
    return lookupImpl(*this, context, name);
}

template <class Self>
Expected<std::reference_wrapper<Info const>>
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
        return std::cref(*info);
    }
    Expected<ParsedRef> const expRef = parseRef(name);
    if (!expRef)
    {
        return Unexpected(formatError("Failed to parse '{}'\n     {}", name, expRef.error().reason()));
    }
    ParsedRef const& ref = *std::move(expRef);
    Info const* res = lookupImpl(self, contextId, ref, name, false);
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
    return std::cref(*res);
}

template <class Self>
Info const*
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

    Info const* contextPtr = self.find(contextId);
    MRDOCS_CHECK_OR(contextPtr, nullptr);
    report::trace("    Context: '{}'", contextPtr->Name);

    // Check the current contextId
    Info const* curContext = contextPtr;
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
    Info const& contextInfo = *contextPtr;
    Info const* res = lookupImpl(self, contextInfo.Parent, ref, name, true);
    self.lookupCacheSet(contextId, name, res);
    return res;
}

Info const*
CorpusImpl::
lookupImpl(
    SymbolID const& contextId,
    ParsedRefComponent const& component,
    ParsedRef const& ref,
    bool const checkParameters) const
{
    report::trace("Looking up component '{}'", component.Name);
    // 1. Find context
    Info const* contextPtr = this->find(contextId);
    MRDOCS_CHECK_OR(contextPtr, nullptr);
    report::trace("    Context: '{}'", contextPtr->Name);

    // If context is a typedef, lookup in the resolved type
    if (auto* TI = dynamic_cast<TypedefInfo const*>(contextPtr))
    {
        MRDOCS_CHECK_OR(TI->Type, nullptr);
        SymbolID resolvedSymbolID = TI->Type->namedSymbol();
        contextPtr = this->find(resolvedSymbolID);
        MRDOCS_CHECK_OR(contextPtr, nullptr);
    }
    Info const& context = *contextPtr;

    // 2. Get a list of members
    report::trace("    Finding members of context '{}'", contextPtr->Name);
    SmallVector<SymbolID, 128> memberIDs;
    auto pushAllMembersOf = [&](Info const& I)
    {
        visit(I, [&]<typename CInfoTy>(CInfoTy const& C) -> Info const*
        {
            if constexpr (InfoParent<CInfoTy>)
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
        Info const* I = find(id);
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
            MatchLevel::TemplateArgsSize :
            MatchLevel::Qualifiers;
    auto matchLevel = MatchLevel::None;
    Info const* res = nullptr;
    for (SymbolID const& memberID : memberIDs)
    {
        Info const* memberPtr = find(memberID);
        MRDOCS_CHECK_OR_CONTINUE(memberPtr);
        Info const& member = *memberPtr;
        report::trace("    Attempting to match {} '{}'", toString(member.Kind), member.Name);
        MatchLevel const memberMatchLevel =
            visit(member, [&]<typename MInfoTy>(MInfoTy const& M)
                -> MatchLevel
        {
            auto matchRes = MatchLevel::None;

            // Name match
            if constexpr (
                std::same_as<MInfoTy, FunctionInfo> ||
                std::same_as<MInfoTy, OverloadsInfo>)
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

            // Template arguments match
            if constexpr (requires { M.Template; })
            {
                std::optional<TemplateInfo> const& templateInfo = M.Template;
                if (component.TemplateArguments.empty())
                {
                    MRDOCS_CHECK_OR(
                        !templateInfo.has_value() ||
                        templateInfo->Args.empty(), matchRes);
                }
                else
                {
                    MRDOCS_CHECK_OR(
                        templateInfo.has_value() &&
                        templateInfo->Args.size() == component.TemplateArguments.size(), matchRes);
                }
            }
            else
            {
                MRDOCS_CHECK_OR(component.TemplateArguments.empty(), matchRes);
            }
            matchRes = MatchLevel::TemplateArgsSize;

            // Function parameters size match
            MRDOCS_CHECK_OR(checkParameters && ref.HasFunctionParameters, matchRes);
            MRDOCS_CHECK_OR(MInfoTy::isFunction(), matchRes);
            auto const& F = dynamic_cast<FunctionInfo const&>(M);
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
                auto& lhsType = F.Params[i].Type;
                auto& rhsType  = ref.FunctionParameters[i];
                MRDOCS_CHECK_OR(isDecayedEqual(lhsType, rhsType), matchRes);
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
            // Early exit if the matchMatchLevel is the highest possible
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
        Info const* memberPtr = find(memberID);
        MRDOCS_CHECK_OR_CONTINUE(memberPtr);
        Info const& member = *memberPtr;
        MRDOCS_CHECK_OR_CONTINUE(isTransparent(member));
        if (Info const* r = lookupImpl(member.id, component, ref, checkParameters))
        {
            return r;
        }
    }

    // If we didn't find a match, return nullptr
    return nullptr;
}

std::pair<Info const*, bool>
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
    Info const* info)
{
    lookupCache_[contextId][std::string(name)] = info;
}


//------------------------------------------------

mrdocs::Expected<std::unique_ptr<Corpus>>
CorpusImpl::
build(
    std::shared_ptr<ConfigImpl const> const& config,
    tooling::CompilationDatabase const& compilations)
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
    // InfoSet in the execution context.
    InfoExecutionContext context(*config);

    // Create an `ASTActionFactory` to create multiple
    // `ASTAction`s that extract the AST for each translation unit.
    std::unique_ptr<tooling::FrontendActionFactory> action =
        makeFrontendActionFactory(context, *config);
    MRDOCS_ASSERT(action);

    // ------------------------------------------
    // "Process file" task
    // ------------------------------------------
    auto const processFile =
        [&](std::string path)
        {
            // Each thread gets an independent copy of a VFS to allow different
            // concurrent working directories.
            IntrusiveRefCntPtr<llvm::vfs::FileSystem> FS =
                llvm::vfs::createPhysicalFileSystem();

            // KRYSTIAN NOTE: ClangTool applies the SyntaxOnly, StripOutput,
            // and StripDependencyFile argument adjusters
            tooling::ClangTool Tool(compilations, { path },
                std::make_shared<PCHContainerOperations>(), FS);

            // Suppress error messages from the tool
            Tool.setPrintErrorMessage(false);

            if (Tool.run(action.get()))
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

    report::info(
        "Extracted {} declarations in {}",
        corpus->info_.size(),
        format_duration(clock_type::now() - start_time));

    // ------------------------------------------
    // Finalize corpus
    // ------------------------------------------
    corpus->finalize();

    return corpus;
}

void
CorpusImpl::
qualifiedName(Info const& I, std::string& result) const
{
    result.clear();
    if (!I.id || I.id == SymbolID::global)
    {
        return;
    }

    if (I.Parent &&
        I.Parent != SymbolID::global)
    {
        if (Info const* PI = find(I.Parent))
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
CorpusImpl::finalize()
{
    {
        report::debug("Finalizing namespaces");
        NamespacesFinalizer finalizer(*this);
        finalizer.build();
    }

    if (config->inheritBaseMembers != PublicSettings::BaseMemberInheritance::Never)
    {
        report::debug("Finalizing base members");
        BaseMembersFinalizer finalizer(*this);
        finalizer.build();
    }

    if (config->overloads)
    {
        report::debug("Finalizing overloads");
        OverloadsFinalizer finalizer(*this);
        finalizer.build();
    }

    if (config->sortMembers)
    {
        report::debug("Finalizing sorted members");
        SortMembersFinalizer finalizer(*this);
        finalizer.build();
    }

    // Finalize javadoc
    report::debug("Finalizing javadoc");
    JavadocFinalizer finalizer(*this);
    finalizer.build();
}


} // clang::mrdocs
