//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// A plugin built the way a plugin author builds one: out of tree, against
// nothing but an installed MrDocs. It stays deliberately small, since what
// is under test is the installation rather than the generator: compiling it
// exercises the headers the package ships and the usage requirements the
// imported tool carries, and linking it exercises the symbols the tool
// exports.

#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Plugin.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string_view>

namespace {

class ProbeGenerator final
    : public mrdocs::Generator
{
public:
    std::string_view
    id() const noexcept override
    {
        return "consumer-probe";
    }

    std::string_view
    displayName() const noexcept override
    {
        return "Consumer probe";
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

mrdocs::Expected<void>
ProbeGenerator::
build(
    mrdocs::Corpus const& corpus,
    mrdocs::Config const& config) const
{
    std::filesystem::path dir(config.configDir());
    dir /= config.output;
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    std::filesystem::path const file = dir / "probe.txt";

    mrdocs::Expected<void> result;
    std::ofstream os(file);
    if (!os)
    {
        result = mrdocs::Unexpected(mrdocs::formatError(
            "could not open \"{}\" for writing", file.string()));
    }
    else
    {
        os << corpus.size() << '\n';
    }
    return result;
}

} // (anon)

MRDOCS_PLUGIN_MAIN(context)
{
    return context.installGenerator(std::make_unique<ProbeGenerator>());
}
