//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "AST/Bitcode.hpp"
#include "AST/FrontendAction.hpp"
#include "CorpusImpl.hpp"
#include "Metadata/Reduce.hpp"
#include "Support/Error.hpp"
#include <mrdox/Metadata.hpp>
#include <mrdox/Support/Error.hpp>
#include <llvm/ADT/STLExtras.h>

namespace clang {
namespace mrdox {

// A standalone function to call to merge a vector of infos into one.
// This assumes that all infos in the vector are of the same type, and will fail
// if they are different.
// Dispatch function.
llvm::Expected<std::unique_ptr<Info>>
mergeInfos(std::vector<std::unique_ptr<Info>>& Values)
{
    if (Values.empty() || !Values[0])
        return llvm::createStringError(llvm::inconvertibleErrorCode(),
            "no info values to merge");

    switch (Values[0]->Kind) {
    case InfoKind::Namespace:
        return reduce<NamespaceInfo>(Values);
    case InfoKind::Record:
        return reduce<RecordInfo>(Values);
    case InfoKind::Enum:
        return reduce<EnumInfo>(Values);
    case InfoKind::Function:
        return reduce<FunctionInfo>(Values);
    case InfoKind::Typedef:
        return reduce<TypedefInfo>(Values);
    case InfoKind::Variable:
        return reduce<VariableInfo>(Values);
    case InfoKind::Field:
        return reduce<FieldInfo>(Values);
    case InfoKind::Specialization:
        return reduce<SpecializationInfo>(Values);
    default:
        return llvm::createStringError(llvm::inconvertibleErrorCode(),
            "unexpected info type");
    }
}

//------------------------------------------------

Info*
CorpusImpl::
find(
    SymbolID const& id) noexcept
{
    auto it = InfoMap.find(StringRef(id));
    if(it != InfoMap.end())
        return it->second.get();
    return nullptr;
}

Info const*
CorpusImpl::
find(
    SymbolID const& id) const noexcept
{
    auto it = InfoMap.find(StringRef(id));
    if(it != InfoMap.end())
        return it->second.get();
    return nullptr;
}

//------------------------------------------------

void
CorpusImpl::
insert(std::unique_ptr<Info> I)
{
    if(I->javadoc)
        I->javadoc->postProcess();

    std::lock_guard<llvm::sys::Mutex> Guard(mutex_);

    index_.emplace_back(I.get());

    // This has to come last because we move I.
    InfoMap[StringRef(I->id)] = std::move(I);
}

//------------------------------------------------

mrdox::Expected<std::unique_ptr<Corpus>>
CorpusImpl::
build(
    tooling::ToolExecutor& ex,
    std::shared_ptr<Config const> config_)
{
    auto config = std::dynamic_pointer_cast<ConfigImpl const>(config_);
    auto corpus = std::make_unique<CorpusImpl>(config);

    // Traverse the AST for all translation units
    // and emit serializd bitcode into tool results.
    // This operation happens ona thread pool.
    if(corpus->config.verboseOutput)
        reportInfo("Mapping declarations");
    if(auto err = ex.execute(
        makeFrontendActionFactory(
            *ex.getExecutionContext(), *config)))
    {
        if(! corpus->config.ignoreFailures)
            return toError(std::move(err));
        reportWarning("warning: mapping failed because ", toString(std::move(err)));
    }

    // Inject the global namespace
    {
        // default-constructed NamespaceInfo
        // describes the global namespace
        NamespaceInfo I;
        insertBitcode(
            *ex.getExecutionContext(),
            writeBitcode(I));
    }

    // Collect the symbols. Each symbol will have
    // a vector of one or more bitcodes. These will
    // be merged later.
    if(corpus->config.verboseOutput)
        reportInfo("Collecting symbols");
    auto bitcodes = collectBitcodes(ex);

    // First reducing phase (reduce all decls into one info per decl).
    if(corpus->config.verboseOutput)
        reportInfo("Reducing {} declarations", bitcodes.size());
    std::atomic<bool> GotFailure;
    GotFailure = false;
    auto errors = corpus->config.threadPool().forEach(
        bitcodes,
        [&](auto& Group)
        {
            // One or more Info for the same symbol ID
            std::vector<std::unique_ptr<Info>> Infos;

            // Each Bitcode can have multiple Infos
            for (auto& bitcode : Group.getValue())
            {
                auto infos = readBitcode(bitcode);
                if(! infos)
                {
                    reportError(infos.error(), "read bitcode");
                    GotFailure = true;
                    return;
                }
                std::move(
                    infos->begin(),
                    infos->end(),
                    std::back_inserter(Infos));
            }

            auto merged = mergeInfos(Infos);
            if(! merged)
            {
                reportError(toError(merged.takeError()), "merge metadata");
                GotFailure = true;
                return;
            }

            std::unique_ptr<Info> I(merged.get().release());
            MRDOX_ASSERT(Group.getKey() == StringRef(I->id));
            corpus->insert(std::move(I));
        });
    if(! errors.empty())
        return Error(errors);

    if(corpus->config.verboseOutput)
        llvm::outs() << "Collected " << corpus->InfoMap.size() << " symbols.\n";

    if(GotFailure)
        return formatError("multiple errors occurred");

    return corpus;
}

} // mrdox
} // clang
