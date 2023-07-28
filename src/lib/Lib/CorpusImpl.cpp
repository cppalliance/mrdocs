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

#include "lib/AST/Bitcode.hpp"
#include "lib/AST/ASTVisitor.hpp"
#include "CorpusImpl.hpp"
#include "lib/Metadata/Reduce.hpp"
#include "lib/Support/Error.hpp"
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
    std::lock_guard<llvm::sys::Mutex> Guard(mutex_);

    index_.emplace_back(I.get());

    // This has to come last because we move I.
    InfoMap[StringRef(I->id)] = std::move(I);
}

//------------------------------------------------

mrdox::Expected<std::unique_ptr<Corpus>>
CorpusImpl::
build(
    ToolExecutor& ex,
    std::shared_ptr<ConfigImpl const> config)
{
    auto corpus = std::make_unique<CorpusImpl>(config);

    // Traverse the AST for all translation units
    // and emit serializd bitcode into tool results.
    // This operation happens ona thread pool.
    report::print(ex.getReportLevel(), "Mapping declarations");
    if(Error err = toError(ex.execute(
        makeFrontendActionFactory(
            *ex.getExecutionContext(), *config))))
    {
        if(! (*config)->ignoreFailures)
            return err;
        report::warn(
            "Warning: mapping failed because ", err);
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
    report::print(ex.getReportLevel(), "Collecting symbols");
    auto bitcodes = collectBitcodes(ex);

    // First reducing phase (reduce all decls into one info per decl).
    report::format(ex.getReportLevel(),
        "Reducing {} declarations", bitcodes.size());
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
                    report::error("{}: reading bitcode", infos.error());
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
                report::error("{}: merging metadata", toError(merged.takeError()));
                GotFailure = true;
                return;
            }

            std::unique_ptr<Info> I(merged.get().release());
            MRDOX_ASSERT(Group.getKey() == StringRef(I->id));
            corpus->insert(std::move(I));
        });
    if(! errors.empty())
        return Error(errors);

    report::format(ex.getReportLevel(),
        "Symbols collected: {}", corpus->InfoMap.size());

    if(GotFailure)
        return formatError("multiple errors occurred");

    return corpus;
}

} // mrdox
} // clang
