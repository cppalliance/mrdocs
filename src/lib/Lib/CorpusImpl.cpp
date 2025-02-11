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
#include "lib/Support/Error.hpp"
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
template <bool checkTemplateArguments, bool checkFunctionParameters>
Info const*
memberMatches(
    InfoSet const& info,
    SymbolID const& MId,
    ParsedRefComponent const& c,
    ParsedRef const& s)
{
    auto const memberIt = info.find(MId);
    MRDOCS_CHECK_OR(memberIt != info.end(), nullptr);
    Info const* memberPtr = memberIt->get();
    MRDOCS_CHECK_OR(memberPtr, nullptr);
    Info const& member = *memberPtr;

    auto isMatch = visit(member, [&]<typename MInfoTy>(MInfoTy const& M)
    {
        // Name match
        if constexpr (requires { { M.OverloadedOperator } -> std::same_as<OperatorKind>; })
        {
            if (c.Operator != OperatorKind::None)
            {
                MRDOCS_CHECK_OR(M.OverloadedOperator == c.Operator, false);
            }
            else
            {
                MRDOCS_CHECK_OR(member.Name == c.Name, false);
            }
        }
        else
        {
            MRDOCS_CHECK_OR(member.Name == c.Name, false);
        }

        if constexpr (checkTemplateArguments)
        {
            if constexpr (requires { M.Template; })
            {
                std::optional<TemplateInfo> const& templateInfo = M.Template;
                MRDOCS_CHECK_OR(templateInfo.has_value(), false);
                MRDOCS_CHECK_OR(templateInfo->Params.size() == c.TemplateArguments.size(), false);
            }
            else
            {
                return false;
            }
        }

        if constexpr (checkFunctionParameters)
        {
            if constexpr (MInfoTy::isFunction())
            {
                MRDOCS_CHECK_OR(M.Params.size() == s.FunctionParameters.size(), false);
            }
            else
            {
                return false;
            }
        }

        return true;
    });
    if (isMatch)
    {
        return memberPtr;
    }
    return nullptr;
}

template <bool checkTemplateArguments, bool checkFunctionParameters>
Info const*
findMemberMatch(
    InfoSet const& info,
    Info const& context,
    ParsedRefComponent const& c,
    ParsedRef const& s)
{
    if constexpr (checkTemplateArguments)
    {
        MRDOCS_CHECK_OR(c.isSpecialization(), nullptr);
    }
    if constexpr (checkFunctionParameters)
    {
        MRDOCS_CHECK_OR(!s.FunctionParameters.empty(), nullptr);
    }

    return visit(context, [&]<typename CInfoTy>(CInfoTy const& C)
        -> Info const*
    {
        if constexpr (InfoParent<CInfoTy>)
        {
            for (SymbolID const& MId : allMembers(C))
            {
                auto const memberIt = info.find(MId);
                MRDOCS_CHECK_OR_CONTINUE(memberIt != info.end());
                Info const* memberPtr = memberIt->get();
                MRDOCS_CHECK_OR_CONTINUE(memberPtr);
                Info const& member = *memberPtr;
                if (member.isOverloads())
                {
                    if (Info const* r = findMemberMatch<
                            checkTemplateArguments,
                            checkFunctionParameters>(info, member, c, s))
                    {
                        return r;
                    }
                }

                if (Info const* r = memberMatches<
                        checkTemplateArguments, checkFunctionParameters>(
                            info, MId, c, s))
                {
                    return r;
                }
            }
        }
        return nullptr;
    });
}

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
}

Expected<Info const*>
CorpusImpl::
lookup(SymbolID const& context, std::string_view const name) const
{
    return lookupImpl(*this, context, name);
}

Expected<Info const*>
CorpusImpl::
lookup(SymbolID const& context, std::string_view name)
{
    return lookupImpl(*this, context, name);
}

template <class Self>
Expected<Info const*>
CorpusImpl::
lookupImpl(Self&& self, SymbolID const& context, std::string_view name)
{
    if (name.starts_with("::"))
    {
        return lookupImpl(self, SymbolID::global, name.substr(2));
    }
    if (auto [info, found] = self.lookupCacheGet(context, name); found)
    {
        if (!info)
        {
            return Unexpected(formatError(
                "Failed to find '{}' from context '{}'",
                name,
                self.Corpus::qualifiedName(*self.find(context))));
        }
        return info;
    }
    Expected<ParsedRef> const s = parseRef(name);
    if (!s)
    {
        return Unexpected(formatError("Failed to parse '{}'\n     {}", name, s.error().reason()));
    }
    Info const* res = lookupImpl(self, context, *s, name, false);
    if (!res)
    {
        auto const contextPtr = self.find(context);
        if (!contextPtr)
        {
            return Unexpected(formatError("Failed to find '{}'", context));
        }
        return Unexpected(formatError(
            "Failed to find '{}' from context '{}'",
            name,
            self.Corpus::qualifiedName(*contextPtr)));
    }
    return res;
}

template <class Self>
Info const*
CorpusImpl::
lookupImpl(
    Self&& self,
    SymbolID const& contextId,
    ParsedRef const& s,
    std::string_view name,
    bool const cache)
{
    if (cache)
    {
        if (auto [info, found] = self.lookupCacheGet(contextId, name); found)
        {
            return info;
        }
    }

    Info const* contextPtr = self.find(contextId);
    MRDOCS_CHECK_OR(contextPtr, nullptr);

    // Check the current contextId
    Info const* cur = contextPtr;
    for (std::size_t i = 0; i < s.Components.size(); ++i)
    {
        ParsedRefComponent const& c = s.Components[i];
        cur = self.lookupImpl(cur->id, c, s, i == s.Components.size() - 1);
        if (!cur)
        {
            break;
        }
    }
    if (cur)
    {
        self.lookupCacheSet(contextId, name, cur);
        return cur;
    }

    // Fallback to parent contextId
    Info const& contextInfo = *contextPtr;
    Info const* res = lookupImpl(self, contextInfo.Parent, s, name, true);
    self.lookupCacheSet(contextId, name, res);
    return res;
}

Info const*
CorpusImpl::
lookupImpl(
    SymbolID const& contextId,
    ParsedRefComponent const& c,
    ParsedRef const& s,
    bool const checkParameters) const
{
    // Find context
    auto const contextIt = info_.find(contextId);
    MRDOCS_CHECK_OR(contextIt != info_.end(), nullptr);
    Info const* contextPtr = contextIt->get();
    MRDOCS_CHECK_OR(contextPtr, nullptr);
    Info const& context = *contextPtr;

    if (checkParameters)
    {
        if (Info const* r = findMemberMatch<true, true>(info_, context, c, s))
        {
            return r;
        }

        if (Info const* r = findMemberMatch<false, true>(info_, context, c, s))
        {
            return r;
        }
    }

    if (Info const* r = findMemberMatch<true, false>(info_, context, c, s))
    {
        return r;
    }

    if (Info const* r = findMemberMatch<false, false>(info_, context, c, s))
    {
        return r;
    }

    // Fallback to members of resolved typedefs
    if (context.isTypedef())
    {
        auto* TI = dynamic_cast<TypedefInfo const*>(&context);
        return lookupImpl(TI->Type->namedSymbol(), c, s, checkParameters);
    }

    // Fallback to transparent contexts
    Info const* r = visit(context, [&]<typename InfoTy>(
        InfoTy const& I) -> Info const*
    {
        if constexpr (InfoParent<InfoTy>)
        {
            for (SymbolID const& MId : allMembers(I))
            {
                Info const* memberPtr = find(MId);
                MRDOCS_CHECK_OR(memberPtr, nullptr);
                Info const& member = *memberPtr;
                if (isTransparent(member))
                {
                    if (auto* r1 = lookupImpl(MId, c, s, checkParameters))
                    {
                        return r1;
                    }
                }
            }
        }
        return nullptr;
    });
    if (r)
    {
        return r;
    }

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
    SymbolID const& context,
    std::string_view name,
    Info const* info)
{
    lookupCache_[context][std::string(name)] = info;
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
