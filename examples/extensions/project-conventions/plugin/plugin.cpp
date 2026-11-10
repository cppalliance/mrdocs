//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// The C++ version of the project-conventions transform on the Corpus
// Transforms page: it warns about each symbol whose brief reads like a stray
// comment instead of a description.

#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Extensions/TransformExtensionContext.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Plugin.hpp>
#include <mrdocs/Support/Reporter.hpp>
#include <mrdocs/Transform.hpp>
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>

namespace {

// tag::is-low-quality[]
bool
isLowQuality(std::string const& brief)
{
    std::size_t special = 0;
    std::size_t total = 0;
    for (char c : brief)
    {
        if (c == ' ')
        {
            continue;
        }
        ++total;
        if (!std::isalnum(static_cast<unsigned char>(c)))
        {
            ++special;
        }
    }
    return !brief.empty() && (brief.size() < 4 || special > total / 2);
}
// end::is-low-quality[]

// tag::check-brief[]
void
warnIfLowQuality(mrdocs::report::Reporter& report, mrdocs::Symbol const& symbol)
{
    if (!symbol.doc || !symbol.doc->brief)
    {
        return;
    }
    std::string brief = mrdocs::doc::getAsPlainText(*symbol.doc->brief);
    brief.erase(0, brief.find_first_not_of(" \t\n\r\f\v"));
    brief.erase(brief.find_last_not_of(" \t\n\r\f\v") + 1);
    if (isLowQuality(brief))
    {
        report.warn(
            "{}: low-quality brief: \"{}\"", symbol.Name, brief);
    }
}
// end::check-brief[]

} // (anon)

MRDOCS_PLUGIN_MAIN(plugin)
{
// tag::transform[]
    return plugin.registerTransform("project-conventions",
        [](mrdocs::TransformExtensionContext const& ctx) -> mrdocs::Expected<void>
        {
            for (mrdocs::Symbol const& symbol : *ctx.corpus)
            {
                warnIfLowQuality(*ctx.report, symbol);
            }
            return {};
        });
// end::transform[]
}
