//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "ScriptGenerator.hpp"
#include "ScriptRunner.hpp"
#include "OutputSink.hpp"
#include <lib/Gen/GeneratorManifest.hpp>
#include <lib/Gen/hbs/AddonPaths.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Metadata/Symbol.hpp>
#include <mrdocs/Metadata/Symbol/SymbolID.hpp>
#include <mrdocs/Support/Path.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mrdocs::script {

namespace {

// Build the read-only corpus DOM a `generate(corpus, output)` entry
// point receives. This mirrors what an extension script sees: a
// `symbols` array of lazy per-symbol objects, each tagged with its flat
// `_id` so a script can form stable per-symbol URLs.
dom::Value
buildScriptCorpus(Corpus const& corpus, DomCorpus const& domCorpus)
{
    dom::Array symbols;
    for (Symbol const& sym : corpus)
    {
        dom::Value value = domCorpus.get(sym.id);
        value.getObject().set("_id", toBase16Str(sym.id));
        symbols.emplace_back(std::move(value));
    }
    dom::Object corpusObj;
    corpusObj.set("symbols", std::move(symbols));
    return dom::Value(std::move(corpusObj));
}

} // (anon)

ScriptGenerator::
ScriptGenerator(std::string id, std::string scriptPath, dom::Object params)
    : id_(std::move(id))
    , scriptPath_(std::move(scriptPath))
    , params_(std::move(params))
{
}

std::string_view
ScriptGenerator::
id() const noexcept
{
    return id_;
}

std::string_view
ScriptGenerator::
displayName() const noexcept
{
    return id_;
}

std::string_view
ScriptGenerator::
fileExtension() const noexcept
{
    // A script-driven generator names its own output files, so there's
    // no single extension. Report the id for diagnostics.
    return id_;
}

Expected<void>
ScriptGenerator::
build(std::string_view outputPath, Corpus const& corpus) const
{
    OutputSink sink(outputPath);
    DomCorpus domCorpus(corpus);
    dom::Value corpusValue = buildScriptCorpus(corpus, domCorpus);
    dom::Value const config(corpus.config.object());
    dom::Value const params(params_);
    Expected<void> result;
    if (scriptPath_.ends_with(".lua"))
    {
        result = runLuaGenerator(
            corpusValue, scriptPath_, sink, config, params);
    }
    else if (scriptPath_.ends_with(".js"))
    {
        result = runJsGenerator(
            corpusValue, scriptPath_, sink, config, params);
    }
    else
    {
        result = Unexpected(formatError(
            "generator '{}': script '{}' must be a .lua or .js file",
            id_, scriptPath_));
    }
    return result;
}

Expected<void>
ScriptGenerator::
buildOne(std::ostream&, Corpus const&) const
{
    return Unexpected(formatError(
        "generator '{}' is script-driven and does not support "
        "single-page output", id_));
}

Expected<void>
discoverScriptGenerators(Config::Settings const& settings)
{
    MRDOCS_TRY(
        std::vector<DiscoveredManifest> found,
        discoverGeneratorManifests(hbs::addon_paths::addonRoots(settings)));
    for (DiscoveredManifest const& d : found)
    {
        // Only manifests that name a `script` are script-driven
        // generators; the data-driven pass installs the rest.
        // First-writer-wins, exactly as the data-driven pass: a
        // duplicate id is a silent skip, and we never pass a `null`.
        if (d.manifest.script)
        {
            std::string const name(files::getFileName(d.dir));
            std::string scriptPath =
                files::appendPath(d.dir, *d.manifest.script);
            (void)installGenerator(
                std::make_unique<ScriptGenerator>(
                    name, std::move(scriptPath), d.manifest.params));
        }
    }
    return {};
}

} // mrdocs::script
