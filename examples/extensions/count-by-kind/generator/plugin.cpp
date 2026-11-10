//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// A MrDocs plugin: a shared library that MrDocs loads as it starts up. This
// one registers a generator, which counts the extracted symbols by kind and
// those that have a brief. The generator is a function registered under an id.

#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Extensions/GeneratorExtensionContext.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Plugin.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <string_view>

namespace {

// Count the symbols of the corpus by kind, ordered by kind name.
std::map<std::string_view, int>
countByKind(mrdocs::Corpus const& corpus)
{
    std::map<std::string_view, int> counts;
    for (mrdocs::Symbol const& symbol : corpus)
    {
        ++counts[mrdocs::toString(symbol.Kind)];
    }
    return counts;
}

// Count the symbols of the corpus that have a brief.
int
countWithBrief(mrdocs::Corpus const& corpus)
{
    int count = 0;
    for (mrdocs::Symbol const& symbol : corpus)
    {
        if (symbol.doc && symbol.doc->brief)
        {
            ++count;
        }
    }
    return count;
}

// A generator that writes one line per symbol kind, with the kind and how
// many symbols of that kind the corpus has. It ends with a line that says how
// many symbols have a brief.
// tag::generator[]
mrdocs::Expected<void>
writeStats(mrdocs::GeneratorExtensionContext const& ctx)
{
    std::filesystem::path const dir(ctx.outputPath);
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    std::filesystem::path const file = dir / "stats.txt";
    std::ofstream os(file);
    if (!os)
    {
        return mrdocs::Unexpected(mrdocs::formatError(
            "could not open \"{}\" for writing", file.string()));
    }
    for (auto const& [kind, count] : countByKind(*ctx.corpus))
    {
        os << kind << ' ' << count << '\n';
    }
    os << "with-brief " << countWithBrief(*ctx.corpus) << '\n';
    return {};
}
// end::generator[]

} // (anon)

// tag::main[]
MRDOCS_PLUGIN_MAIN(plugin)
{
// tag::register-generator[]
    return plugin.registerGenerator(
        "stats", writeStats, "Symbol statistics", "txt");
// end::register-generator[]
}
// end::main[]
