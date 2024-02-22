//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "ExecutionContext.hpp"
#include "lib/AST/Bitcode.hpp"
#include "lib/Metadata/Reduce.hpp"
#include <mrdocs/Metadata.hpp>
#include <ranges>

namespace clang {
namespace mrdocs {

namespace {

/** Merges a vector of Info objects.

    This function is used to merge a vector of Info objects with the same
    SymbolID. The function assumes that all Info objects are of the same type.
    If they are not, the function will fail.

    @param Values The vector of Info objects to merge.
*/
mrdocs::Expected<std::unique_ptr<Info>>
mergeInfos(std::vector<std::unique_ptr<Info>>& Values)
{
    if(Values.empty() || ! Values[0])
    {
        return Unexpected(formatError("no info values to merge"));
    }

    return visit(*Values[0], [&]<typename T>(T&) mutable
        {
            return reduce<T>(Values);
        });
}

/** Merges two Info objects.

    This function is used to merge two Info objects with the same SymbolID.
    The function assumes that the two Info objects are of the same type.
    If they are not, the function will fail.

    @param I The Info object to merge into.
    @param Other The Info object to merge from.
*/
void
merge(Info& I, Info&& Other)
{
    MRDOCS_ASSERT(I.Kind == Other.Kind);
    MRDOCS_ASSERT(I.id == Other.id);
    visit(I, [&]<typename InfoTy>(InfoTy& II) mutable
        {
            merge(II, static_cast<InfoTy&&>(Other));
        });
}

} // (anon)

// ----------------------------------------------------------------
// InfoExecutionContext
// ----------------------------------------------------------------

void
InfoExecutionContext::
report(
    InfoSet&& results,
    Diagnostics&& diags)
{
    InfoSet info = std::move(results);
    // KRYSTIAN TODO: read stage will be required to
    // update Info references once we switch to using Info*
    #if 0
    {
        std::shared_lock<std::shared_mutex> read_lock(mutex_);
    }
    #endif

    std::unique_lock<std::shared_mutex> write_lock(mutex_);
    // Add all new Info to the existing set.
    info_.merge(info);

    // Merge duplicate IDs in info_.
    for (auto& other : info)
    {
        auto it = info_.find(other->id);
        MRDOCS_ASSERT(it != info_.end());
        merge(**it, std::move(*other));
    }

    // Merge diagnostics and report any new messages.
    diags_.mergeAndReport(std::move(diags));
}

void
InfoExecutionContext::
reportEnd(report::Level level)
{
    diags_.reportTotals(level);
}


mrdocs::Expected<InfoSet>
InfoExecutionContext::
results()
{
    return std::move(info_);
}

// ----------------------------------------------------------------
// BitcodeExecutionContext
// ----------------------------------------------------------------

void
BitcodeExecutionContext::
report(
    InfoSet&& results,
    Diagnostics&& diags)
{
    InfoSet info = std::move(results);
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& I : info)
    {
        llvm::SmallString<0> bitcode = writeBitcode(*I);
        auto [it, created] = bitcode_.try_emplace(I->id);
        auto& bitcodes = it->second;
        if (created || std::ranges::find(bitcodes, bitcode) == bitcodes.end())
        {
            bitcodes.emplace_back(std::move(bitcode));
        }
    }

    diags_.mergeAndReport(std::move(diags));
}

void
BitcodeExecutionContext::
reportEnd(report::Level level)
{
    diags_.reportTotals(level);
}

mrdocs::Expected<InfoSet>
BitcodeExecutionContext::
results()
{
    InfoSet result;
    auto errors = config_.threadPool().forEach(
        bitcode_,
        [&](auto& Group)
        {
            auto& [id, bitcodes] = Group;

            // One or more Info for the same symbol ID
            std::vector<std::unique_ptr<Info>> Infos;

            // Each Bitcode can have multiple Infos
            for(auto& bitcode : bitcodes)
            {
                Expected infos = readBitcode(bitcode);
                if (infos.has_value())
                {
                    std::move(
                        infos->begin(),
                        infos->end(),
                        std::back_inserter(Infos));
                }
                else
                {
                    report::error("Failed to read bitcode: {}", infos.error());
                }
            }
            Expected merged = mergeInfos(Infos);
            std::unique_ptr<Info> I = std::move(merged.value());
            MRDOCS_ASSERT(I);
            MRDOCS_ASSERT(id == I->id);
            std::lock_guard<std::mutex> lock(mutex_);
            result.emplace(std::move(I));
        });

    if (!errors.empty())
    {
        return Unexpected(errors);
    }
    return result;
}


} // mrdocs
} // clang
