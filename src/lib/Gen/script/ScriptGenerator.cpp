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
#include "OutputSink.hpp"

#include <mrdocs/Corpus.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Metadata/Symbol.hpp>
#include <mrdocs/Metadata/Symbol/SymbolID.hpp>

#include <utility>

namespace mrdocs::script {

namespace {

// Build the read-only corpus the generator's `ctx.corpus` exposes: a
// `symbols` array of the same per-symbol objects the templates see, each
// tagged with its flat `_id` so a generator can form stable per-symbol
// URLs.
dom::Value
buildGeneratorCorpus(Corpus const& corpus, DomCorpus const& domCorpus)
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

// Build the `ctx.output` object and its `write(path, contents)` method.
// The method is a DOM invocable that routes to the sink; the same value
// is callable from both Lua and JavaScript, so one output API serves
// either language. The sink outlives the call (a local in
// `runScriptGenerator`), so capturing it by pointer is safe.
dom::Value
buildOutputApi(OutputSink& sink)
{
    OutputSink* sinkPtr = &sink;
    dom::Object api;
    api.set("write", dom::Value(dom::makeVariadicInvocable(
        [sinkPtr](dom::Array const& args) -> Expected<dom::Value, Error>
        {
            Expected<dom::Value, Error> result;
            if (args.size() < 2 ||
                !args.get(0).isString() ||
                !args.get(1).isString())
            {
                result = Unexpected(Error(
                    "output.write: expected (string path, string contents)"));
            }
            else if (Expected<void> wrote = sinkPtr->write(
                         args.get(0).getString().get(),
                         args.get(1).getString().get());
                     !wrote)
            {
                result = Unexpected(wrote.error());
            }
            else
            {
                result = dom::Value();
            }
            return result;
        })));
    return dom::Value(std::move(api));
}

// The generator's own options block, from `generator-options.<id>`, handed
// to the script as `ctx.params`; an empty object when the id has none.
dom::Object
generatorParams(Corpus const& corpus, std::string_view id)
{
    auto const& genOpts = corpus.config->generatorOptions;
    auto const it = genOpts.find(std::string(id));
    return it != genOpts.end() ? it->second : dom::Object();
}

// Assemble the single `ctx` object the generator receives.
dom::Value
buildGeneratorContext(
    Corpus const& corpus,
    DomCorpus const& domCorpus,
    OutputSink& sink,
    std::string_view id)
{
    dom::Object ctx;
    ctx.set("corpus", buildGeneratorCorpus(corpus, domCorpus));
    ctx.set("output", buildOutputApi(sink));
    ctx.set("config", dom::Value(corpus.config.object()));
    ctx.set("params", dom::Value(generatorParams(corpus, id)));
    return dom::Value(std::move(ctx));
}

} // (anon)

Expected<void>
runScriptGenerator(
    dom::Function const& generate,
    std::string_view id,
    Corpus const& corpus,
    std::string_view outputPath)
{
    OutputSink sink(outputPath);
    DomCorpus domCorpus(corpus);
    dom::Value ctx = buildGeneratorContext(corpus, domCorpus, sink, id);

    Expected<dom::Value> invoked = generate.try_invoke(ctx);
    Expected<void> result;
    if (!invoked)
    {
        result = Unexpected(formatError(
            "generator '{}': {}", id, invoked.error().message()));
    }
    return result;
}

} // mrdocs::script
