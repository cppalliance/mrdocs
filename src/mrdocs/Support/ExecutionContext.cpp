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

#include "ExecutionContext.hpp"
#include <mrdocs/Metadata/Reduce.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Metadata/Symbol/Namespace.hpp>
#include <mrdocs/Support/Error/Assert.hpp>
#include <mrdocs/Support/Reflection/MergeReflectedType.hpp>
#include <mrdocs/Support/Report.hpp>
#include <ranges>


namespace mrdocs {

namespace {

#if 0
/** Merges a vector of Info objects.

    This function is used to merge a vector of Info objects with the same
    SymbolID. The function assumes that all Info objects are of the same type.
    If they are not, the function will fail.

    @param Values The vector of Info objects to merge.
*/
mrdocs::Expected<std::unique_ptr<Symbol>>
mergeInfos(std::vector<std::unique_ptr<Symbol>>& Values)
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
// ExecutionContext
// ----------------------------------------------------------------

void
ExecutionContext::
report(
    detail::SymbolSet&& results,
    Diagnostics&& diags,
    detail::UndocumentedSymbolSet&& undocumented)
{
    detail::SymbolSet info = std::move(results);
    std::unique_lock<std::shared_mutex> write_lock(mutex_);

    // Add all new Info to the existing set.
    info_.merge(info);

    // Merge duplicate IDs in info_.
    for (auto& other : info)
    {
        auto it = info_.find(other->id);
        MRDOCS_ASSERT(it != info_.end());
        visit(**it, [&]<typename T>(T& target) {
            auto *source = dynamic_cast<T*>(other.get());
            MRDOCS_ASSERT(source);
            merge(target, std::move(*source));
        });
    }

    // Merge diagnostics and report any new messages.
    diags_.mergeAndReport(std::move(diags));

    // Merge undocumented symbols and remove any symbols
    // from undocumented that we can find in info_ with
    // documentation from other translation units.
    undocumented_.merge(undocumented);
    std::size_t certainCount = 0;
    for (auto it = undocumented_.begin(); it != undocumented_.end();)
    {
        if (auto infoIt = info_.find(it->id);
            infoIt != info_.end() &&
            infoIt->get()->doc)
        {
            it = undocumented_.erase(it);
        }
        else
        {
            if (it->certainToWarn)
            {
                ++certainCount;
            }
            ++it;
        }
    }

    // max-errors > 0 (only with warn-as-error): once we have found that many
    // undocumented symbols, stop looking. Signal the build loop to stop
    // dispatching new translation units (in-flight ones still finish, so the
    // merged set may run a little past the cap; that overshoot is fine).
    // Only symbols certain to produce a warning count toward the budget;
    // symbols that finalization may still fold or document are recorded but
    // must not stop extraction, or the run could halt with nothing to report.
    if (config_.warnAsError
        && config_.maxErrors > 0
        && certainCount >= config_.maxErrors
        && !stopExtraction_.exchange(true, std::memory_order_relaxed))
    {
        // This only fires under warn-as-error, and reaching the budget means
        // the run has at least max-errors documentation errors, so the
        // message is itself an error: it marks the run as failed even if the
        // individual diagnostics are pruned before they are emitted.
        report::error(
            "max-errors={}: found enough documentation problems; "
            "translation units not yet started will be skipped",
            config_.maxErrors);
    }
}

void
ExecutionContext::
reportEnd(report::Level level)
{
    diags_.reportTotals(level);
}

Expected<detail::SymbolSet>
ExecutionContext::
results()
{
    return std::move(info_);
}

detail::UndocumentedSymbolSet
ExecutionContext::
undocumented()
{
    return std::move(undocumented_);
}

} // mrdocs

