//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// The C++ version of the subclass-tree transform on the Corpus Transforms
// page: it prints the classes derived from `Shape`, one per line, indented by
// depth.

#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Extensions/TransformExtensionContext.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Plugin.hpp>
#include <mrdocs/Support/Reporter.hpp>
#include <mrdocs/Transform.hpp>
#include <string>
#include <string_view>

namespace {

// tag::list-subclasses[]
void
listSubclasses(mrdocs::report::Reporter& report, mrdocs::Corpus const& corpus,
    mrdocs::RecordSymbol const& record, std::string const& indent)
{
    for (mrdocs::SymbolID const& id : record.Derived)
    {
        auto const* child = corpus.find(id);
        if (!child)
        {
            continue;
        }
        report.info("{}{}", indent, child->Name);
        if (auto const* derived = child->asRecordPtr())
        {
            listSubclasses(report, corpus, *derived, indent + "  ");
        }
    }
}
// end::list-subclasses[]

} // (anon)

MRDOCS_PLUGIN_MAIN(plugin)
{
// tag::transform[]
    return plugin.registerTransform("subclass-tree",
        [](mrdocs::TransformExtensionContext const& ctx) -> mrdocs::Expected<void>
        {
            mrdocs::Corpus const& corpus = *ctx.corpus;
            auto base = corpus.lookup("Shape");
            if (base)
            {
                ctx.report->info("{}", base->Name);
                if (auto const* record = base->asRecordPtr())
                {
                    listSubclasses(*ctx.report, corpus, *record, "  ");
                }
            }
            return {};
        });
// end::transform[]
}
