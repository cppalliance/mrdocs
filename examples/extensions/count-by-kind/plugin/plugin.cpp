//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// The C++ version of the count-by-kind transform on the Corpus Transforms
// page: it counts the symbols of each kind and reports the totals.

#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Extensions/TransformExtensionContext.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Plugin.hpp>
#include <mrdocs/Support/Reporter.hpp>
#include <mrdocs/Transform.hpp>
#include <map>
#include <string_view>

namespace {

} // (anon)

MRDOCS_PLUGIN_MAIN(plugin)
{
// tag::transform[]
    return plugin.registerTransform("count-by-kind",
        [](mrdocs::TransformExtensionContext const& ctx) -> mrdocs::Expected<void>
        {
            std::map<std::string_view, int> counts;
            for (mrdocs::Symbol const& symbol : *ctx.corpus)
            {
                ++counts[mrdocs::toString(symbol.Kind)];
            }
            for (auto const& [kind, count] : counts)
            {
                ctx.report->info("{}: {}", kind, count);
            }
            return {};
        });
// end::transform[]
}
