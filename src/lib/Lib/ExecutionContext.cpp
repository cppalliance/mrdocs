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
#include "lib/Metadata/Reduce.hpp"
#include <mrdocs/Metadata.hpp>
#include <ranges>

namespace clang {
namespace mrdocs {

namespace {

#if 0
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
#endif

} // (anon)

// ----------------------------------------------------------------
// InfoExecutionContext
// ----------------------------------------------------------------

void
InfoExecutionContext::
report(
    InfoSet&& results,
    Diagnostics&& diags,
    UndocumentedInfoSet&& undocumented)
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



    // Merge undocumented symbols and remove any symbols
    // from undocumented that we can find in info_ with
    // documentation from other translation units.
    undocumented_.merge(undocumented);
    for (auto it = undocumented_.begin(); it != undocumented_.end();)
    {
        auto infoIt = info_.find(it->first);
        if (infoIt != info_.end() &&
            infoIt->get()->javadoc)
        {
            it = undocumented_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void
InfoExecutionContext::
reportEnd(report::Level level)
{
    diags_.reportTotals(level);
}

Expected<InfoSet>
InfoExecutionContext::
results()
{
    return std::move(info_);
}

UndocumentedInfoSet
InfoExecutionContext::
undocumented()
{
    return std::move(undocumented_);
}

} // mrdocs
} // clang
