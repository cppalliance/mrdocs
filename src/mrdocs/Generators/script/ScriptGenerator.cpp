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
#include "../../Support/DescribedToDom/CachedDescribedObject.hpp"
#include "../../Support/Report/ReportDom.hpp"
#include "OutputSink.hpp"
#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Dom/OverlayObject.hpp>
#include <mrdocs/Extensions/GeneratorExtensionContext.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Metadata/Symbol.hpp>
#include <mrdocs/Metadata/Symbol/SymbolID.hpp>
#include <mrdocs/Support/DescribedToDom.hpp>
#include <mrdocs/Support/Error/Assert.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <string>
#include <utility>

namespace mrdocs::script {

namespace {

// Build the `ctx.output` object with its `write(path, contents[, append])`
// and `append(path, contents)` methods. Each is a DOM invocable that routes
// to the sink; the same value is callable from both Lua and JavaScript, so
// one output API serves either language. The sink outlives the call (a local
// in `ScriptGenerator::build`), so capturing it by pointer is safe.
//
// `append` lets a generator stream a large artifact in small chunks instead
// of building the whole string in the interpreter heap and writing it once,
// which matters for the small JerryScript heap on a large corpus.
dom::Value
buildOutputApi(OutputSink& sink)
{
    OutputSink* sinkPtr = &sink;
    auto const writeImpl =
        [sinkPtr](dom::Array const& args, bool append)
            -> Expected<dom::Value, dom::Error>
        {
            char const* who = append ? "output.append" : "output.write";
            Expected<dom::Value, dom::Error> result;
            if (args.size() < 2 ||
                !args.get(0).isString() ||
                !args.get(1).isString())
            {
                result = Unexpected(dom::Error(
                    std::string(who) + ": expected (string path, string contents)"));
            }
            else
            {
                // output.write accepts an optional third argument that
                // requests append mode; output.append always appends.
                bool const doAppend = append ||
                    (args.size() >= 3 && args.get(2).isTruthy());
                if (Expected<void> wrote = sinkPtr->write(
                        args.get(0).getString().get(),
                        args.get(1).getString().get(),
                        doAppend);
                    !wrote)
                {
                    result = Unexpected(dom::Error(std::string(wrote.error().message())));
                }
                else
                {
                    result = dom::Value();
                }
            }
            return result;
        };
    dom::Object api;
    api.set("write", dom::Value(dom::makeVariadicInvocable(
        [writeImpl](dom::Array const& args) -> Expected<dom::Value, dom::Error>
        {
            return writeImpl(args, false);
        })));
    api.set("append", dom::Value(dom::makeVariadicInvocable(
        [writeImpl](dom::Array const& args) -> Expected<dom::Value, dom::Error>
        {
            return writeImpl(args, true);
        })));
    return dom::Value(std::move(api));
}

// The `ctx` passed to a registered generator script.
//
// It is the described `GeneratorExtensionContext`, projected through `describedToDom` (see `CachedDescribedObject`)
// (`corpus`, `config`, `outputPath`, `params`), plus the keys no field of the
// type produces, which the binding adds by hand because a function cannot be a
// field of a described type: `ctx.output`, which is `output`,
// `ctx.stringify(value)`, which serializes any value to a compact JSON string
// through the DOM encoder (JavaScript has `JSON.stringify` natively; this
// gives Lua the same, and behaves identically in both), and `ctx.report`, the
// reporter of the run (see `reportDom`). The `output` and `stringify` keys
// move to importable modules under cppalliance/mrdocs#1284.
//
// The object copies `context`; the corpus, config and reporter it points to
// have to outlive it.
dom::Value
generatorExtensionContextDom(GeneratorExtensionContext const& context, dom::Value output)
{
    MRDOCS_ASSERT(context.report);
    dom::Object extras;
    extras.set("output", std::move(output));
    extras.set("report", reportDom(*context.report));
    extras.set("stringify", dom::Value(dom::makeVariadicInvocable([](dom::Array const& args) -> Expected<dom::Value, dom::Error>
                                {
            dom::Value const v =
                args.empty() ? dom::Value(nullptr) : args.get(0);
            return dom::Value(dom::JSON::stringify(v)); })));
    return dom::Value(
        dom::overlayObject(
            extras,
            dom::newObject<CachedDescribedObject<GeneratorExtensionContext>>(context)));
}

} // (anon)

Expected<void>
ScriptGenerator::
build(
    Corpus const& corpus,
    Config const& config,
    report::Reporter& reporter) const
{
    // The generator writes where getGeneratorOutputPath resolves, the same
    // location a generator written in C++ gets as `outputPath`. It owns the
    // whole emit from there.
    GeneratorExtensionContext const context =
        makeGeneratorExtensionContext(*this, corpus, config, reporter);

    OutputSink sink(context.outputPath);
    dom::Value const ctx =
        generatorExtensionContextDom(context, buildOutputApi(sink));

    Expected<dom::Value> invoked = fn_.tryInvoke(ctx);
    Expected<void> result;
    if (!invoked)
    {
        result = Unexpected(Error(invoked.error().message()));
    }
    return result;
}

} // mrdocs::script
