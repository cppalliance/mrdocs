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

#include "ScriptGenerator.hpp"
#include "OutputSink.hpp"
#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Metadata/Symbol.hpp>
#include <mrdocs/Metadata/Symbol/SymbolID.hpp>
#include <mrdocs/Support/DescribedToDom.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>

#include <string>

namespace mrdocs::script {

namespace {

// Build the read-only corpus the generator's `ctx.corpus` exposes,
// mirroring the object an extension script sees (@ref buildCorpusDom):
//
//  - `corpus.symbols`   -- every symbol as its reflection view.
//  - `corpus.get(id)`   -- the symbol for a base58 id, or null.
//  - `corpus.lookup(nm)`-- the symbol for a qualified name in the global
//                          namespace, or null.
//
// Everything is read-only: it goes through `DomCorpus::get`, whose views
// are const, since a generator reads the corpus but does not mutate it.
dom::Value
buildGeneratorCorpus(Corpus const& corpus, DomCorpus const& domCorpus)
{
    // The array is built eagerly rather than as a DescribedArrayProxy:
    // that proxy wraps a homogeneous `std::vector<T>`, but the corpus
    // stores symbols as a heterogeneous, polymorphic index, so it cannot
    // be wrapped directly. Each element is the symbol's reflection view.
    dom::Array symbols;
    for (Symbol const& sym : corpus)
    {
        symbols.emplace_back(domCorpus.get(sym.id));
    }

    // `corpus.get(id)` -- decode the base58 id and return its view.
    auto getFn = dom::makeVariadicInvocable(
        [dc = &domCorpus](dom::Array const& args) -> dom::Expected<dom::Value>
        {
            if (args.size() < 1 || !args.get(0).isString())
            {
                return Unexpected(dom::Error("corpus.get: expected a string id"));
            }
            auto const id = fromBase58Str(args.get(0).getString());
            if (!id)
            {
                return dom::Value(nullptr);
            }
            return dc->get(*id);
        });

    // `corpus.lookup(name)` -- look up a symbol by qualified name in the
    // global namespace, mirroring `Corpus::lookup(name)`.
    auto lookupFn = dom::makeVariadicInvocable(
        [dc = &domCorpus](dom::Array const& args) -> dom::Expected<dom::Value>
        {
            if (args.size() < 1 || !args.get(0).isString())
            {
                return Unexpected(dom::Error("corpus.lookup: expected a string name"));
            }
            Expected<Symbol const&> const result =
                dc->getCorpus().lookup(SymbolID::global, args.get(0).getString());
            if (!result)
            {
                return dom::Value(nullptr);
            }
            return dc->get(result.value().id);
        });

    dom::Object corpusObj;
    corpusObj.set("symbols", std::move(symbols));
    corpusObj.set("get", dom::Value(std::move(getFn)));
    corpusObj.set("lookup", dom::Value(std::move(lookupFn)));
    return {std::move(corpusObj)};
}

// Build the `ctx.output` object and its `write(path, contents)` method.
// The method is a DOM invocable that routes to the sink; the same value
// is callable from both Lua and JavaScript, so one output API serves
// either language. The sink outlives the call (a local in
// `ScriptGenerator::build`), so capturing it by pointer is safe.
dom::Value
buildOutputApi(OutputSink& sink)
{
    OutputSink* sinkPtr = &sink;
    dom::Object api;
    api.set("write", dom::Value(dom::makeVariadicInvocable(
        [sinkPtr](dom::Array const& args) -> Expected<dom::Value, dom::Error>
        {
            Expected<dom::Value, dom::Error> result;
            if (args.size() < 2 ||
                !args.get(0).isString() ||
                !args.get(1).isString())
            {
                result = Unexpected(dom::Error(
                    "output.write: expected (string path, string contents)"));
            }
            else if (Expected<void> wrote = sinkPtr->write(
                         args.get(0).getString().get(),
                         args.get(1).getString().get());
                     !wrote)
            {
                result = Unexpected(dom::Error(std::string(wrote.error().message())));
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
generatorParams(Config const& config, std::string_view id)
{
    auto const& genOpts = config.generatorOptions;
    auto const it = genOpts.find(std::string(id));
    return it != genOpts.end() ? it->second : dom::Object();
}

// Assemble the single `ctx` object the generator receives.
dom::Value
buildGeneratorContext(
    Corpus const& corpus,
    Config const& config,
    DomCorpus const& domCorpus,
    OutputSink& sink,
    std::string_view id)
{
    dom::Object ctx;
    ctx.set("corpus", buildGeneratorCorpus(corpus, domCorpus));
    ctx.set("output", buildOutputApi(sink));
    ctx.set("config", describedToDom(config));
    ctx.set("params", dom::Value(generatorParams(config, id)));
    // `ctx.stringify(value)` -- serialize any value to a compact JSON string
    // through the DOM encoder, so a script can emit JSON without building it
    // by hand. JavaScript has `JSON.stringify` natively; this gives Lua the
    // same, and behaves identically in both. The argument is whatever the
    // script passes (a table/array or a `ctx.corpus` value), converted to a
    // DOM value by the engine before this runs.
    ctx.set("stringify", dom::Value(dom::makeVariadicInvocable(
        [](dom::Array const& args) -> Expected<dom::Value, dom::Error>
        {
            dom::Value const v =
                args.empty() ? dom::Value(nullptr) : args.get(0);
            return dom::Value(dom::JSON::stringify(v));
        })));
    return {std::move(ctx)};
}

} // (anon)

Expected<void>
ScriptGenerator::
build(Corpus const& corpus, Config const& config) const
{
    // The generator writes under the configured output directory, resolved
    // relative to the configuration file. It owns the whole emit from there.
    std::string const outputPath = files::normalizePath(
        files::makeAbsolute(config.output, config.configDir()));

    OutputSink sink(outputPath);
    DomCorpus domCorpus(corpus);
    dom::Value ctx = buildGeneratorContext(corpus, config, domCorpus, sink, id_);

    Expected<dom::Value> invoked = fn_.try_invoke(ctx);
    Expected<void> result;
    if (!invoked)
    {
        result = Unexpected(formatError(
            "generator '{}': {}", id_, invoked.error().message()));
    }
    return result;
}

} // mrdocs::script
