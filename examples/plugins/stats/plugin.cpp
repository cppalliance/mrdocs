//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// A MrDocs plugin: a shared library that MrDocs loads as it starts up
// and that installs a generator counting the extracted symbols by kind.

#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Plugin.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string_view>

namespace {

// tag::generator[]
// A generator that writes one line per symbol kind, indicating the kind and
// how many symbols of that kind the corpus has.
class StatsGenerator final
    : public mrdocs::Generator
{
public:
    std::string_view
    id() const noexcept override
    {
        return "stats";
    }

    std::string_view
    displayName() const noexcept override
    {
        return "Symbol statistics";
    }

    std::string_view
    fileExtension() const noexcept override
    {
        return "txt";
    }

    mrdocs::Expected<void>
    build(
        mrdocs::Corpus const& corpus,
        mrdocs::Config const& config) const override;
};
// end::generator[]

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

// Resolve the directory the generator writes into.
std::filesystem::path
outputDir(mrdocs::Config const& config)
{
    std::filesystem::path dir(config.configDir());
    dir /= config.output;
    return dir;
}

// tag::build[]
mrdocs::Expected<void>
StatsGenerator::
build(
    mrdocs::Corpus const& corpus,
    mrdocs::Config const& config) const
{
    std::map<std::string_view, int> const counts =
        countByKind(corpus);
    std::filesystem::path const dir = outputDir(config);
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    std::filesystem::path const file = dir / "stats.txt";

    mrdocs::Expected<void> result;
    std::ofstream os(file);
    if (!os)
    {
        result = mrdocs::Unexpected(mrdocs::formatError(
            "could not open \"{}\" for writing", file.string()));
    }
    else
    {
        for (auto const& [kind, count] : counts)
        {
            os << kind << ' ' << count << '\n';
        }
    }
    return result;
}
// end::build[]

} // (anon)

// tag::main[]
MRDOCS_PLUGIN_MAIN(context)
{
    return context.installGenerator(std::make_unique<StatsGenerator>());
}
// end::main[]
