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

#include "ExecutionContext.hpp"
#include "lib/AST/Bitcode.hpp"
#include "lib/Metadata/Reduce.hpp"
#include "lib/Metadata/Finalize.hpp"
#include <mrdocs/Metadata.hpp>

namespace clang {
namespace mrdocs {

namespace {

// A standalone function to call to merge a vector of infos into one.
// This assumes that all infos in the vector are of the same type, and will fail
// if they are different.
// Dispatch function.
mrdocs::Expected<std::unique_ptr<Info>>
mergeInfos(std::vector<std::unique_ptr<Info>>& Values)
{
    if(Values.empty() || ! Values[0])
        return Unexpected(formatError("no info values to merge"));

    return visit(*Values[0], [&]<typename T>(T&) mutable
        {
            return reduce<T>(Values);
        });
}

void merge(Info& I, Info&& Other)
{
    MRDOCS_ASSERT(I.Kind == Other.Kind);
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
    // add all new Info to the existing set. after this call,
    // info will only contain duplicates which will require merging
    info_.merge(info);

    for(auto& other : info)
    {
        auto it = info_.find(other->id);
        MRDOCS_ASSERT(it != info_.end());
        merge(**it, std::move(*other));
    }

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
    finalize(info_);
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
    for(auto& I : info)
    {
        auto bitcode = writeBitcode(*I);
        auto [it, created] = bitcode_.try_emplace(I->id);
        if(auto& codes = it->second; created ||
            std::find(codes.begin(), codes.end(), bitcode) == codes.end())
            codes.emplace_back(std::move(bitcode));
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
            // One or more Info for the same symbol ID
            std::vector<std::unique_ptr<Info>> Infos;

            // Each Bitcode can have multiple Infos
            for(auto& bitcode : Group.second)
            {
                auto infos = readBitcode(bitcode);
                std::move(
                    infos->begin(),
                    infos->end(),
                    std::back_inserter(Infos));
            }

            auto merged = mergeInfos(Infos);
            std::unique_ptr<Info> I = std::move(merged.value());
            MRDOCS_ASSERT(I);
            MRDOCS_ASSERT(Group.first == I->id);
            std::lock_guard<std::mutex> lock(mutex_);
            result.emplace(std::move(I));
        });

    if(! errors.empty())
        return Unexpected(errors);
    finalize(result);
    return result;
}


} // mrdocs
} // clang
