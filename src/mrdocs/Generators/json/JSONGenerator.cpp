//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "JSONGenerator.hpp"
#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Dom/Array.hpp>
#include <mrdocs/Dom/Object.hpp>
#include <mrdocs/Dom/Value.hpp>
#include <mrdocs/Handlebars/Engine.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Metadata/Symbol.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Generator.hpp>
#include <concepts>
#include <string_view>
#include <utility>

namespace mrdocs {
namespace json {

namespace {

// Whether a value counts as empty for the compact output. This is the
// Handlebars notion (handlebars::isEmpty: an empty string or array, `null`, or
// `false`, but never a number so `0` stays) plus the empty object, which
// handlebars::isEmpty does not cover because objects are truthy.
bool
isEmptyForJson(dom::Value const& v)
{
    return handlebars::isEmpty(v) ||
        (v.isObject() && v.getObject().empty());
}

// Copy `v`, dropping the reflection `$meta` field unless `includeMeta`, and
// dropping empty entries (see isEmptyForJson) when `!emitEmpty`. Scalars pass
// through unchanged. Filtering is bottom-up, so an object left empty after its
// members are dropped is itself dropped. This is the only shaping the generator
// does; field names and values are otherwise exactly the symbol's DOM.
dom::Value
filterValue(dom::Value const& v, bool const includeMeta, bool const emitEmpty)
{
    if (v.isObject())
    {
        dom::Object out;
        v.getObject().visit(
            [&](dom::String const& key, dom::Value const& val)
            {
                if (!includeMeta && std::string_view(key) == "$meta")
                {
                    return;
                }
                dom::Value fv = filterValue(val, includeMeta, emitEmpty);
                if (!emitEmpty && isEmptyForJson(fv))
                {
                    return;
                }
                out.set(key, std::move(fv));
            });
        return dom::Value(std::move(out));
    }
    if (v.isArray())
    {
        dom::Array out;
        dom::Array const& arr = v.getArray();
        for (std::size_t i = 0; i < arr.size(); ++i)
        {
            dom::Value fv = filterValue(arr.get(i), includeMeta, emitEmpty);
            if (!emitEmpty && isEmptyForJson(fv))
            {
                continue;
            }
            out.push_back(std::move(fv));
        }
        return dom::Value(std::move(out));
    }
    return v;
}

// Read a boolean option from `generator-options.json.<name>`.
bool
boolOption(Config const& config, std::string_view const name, bool const fallback)
{
    auto const it = config.generatorOptions.find("json");
    if (it == config.generatorOptions.end())
    {
        return fallback;
    }
    dom::Object const& opts = it->second;
    std::string const key(name);
    if (!opts.exists(key))
    {
        return fallback;
    }
    return opts.get(key).isTruthy();
}

// Walks the corpus from the global namespace, following each symbol's
// members, exactly as the XML and Handlebars single-page generators do. The
// global namespace and its members are already in a stable, sorted order, so
// this yields a deterministic, platform-independent sequence without sorting.
// operator() is a template over the concrete symbol type so that
// `corpus.traverse(I, *this)` sees that concrete type and visits its members
// (the base `Symbol` is not a `SymbolParent`, so a non-templated visitor
// would stop at the global namespace). Dependency symbols are skipped, as in
// the XML generator.
struct SymbolCollector
{
    Corpus const& corpus;
    dom::Array& symbols;
    bool includeMeta;
    bool emitEmpty;

    template <std::derived_from<Symbol> T>
    void
    operator()(T const& I)
    {
        if (I.Extraction == ExtractionMode::Dependency)
        {
            return;
        }
        dom::Value v = buildSymbolDom(I);
        if (!includeMeta || !emitEmpty)
        {
            v = filterValue(v, includeMeta, emitEmpty);
        }
        symbols.push_back(std::move(v));
        corpus.traverse(I, *this);
    }
};

} // (anon)

Expected<void>
JSONGenerator::
build(Corpus const& corpus, Config const& config) const
{
    // Walk the corpus one symbol at a time and write a single file: an object
    // with a `symbols` array holding each symbol's DOM (the same projection the
    // Handlebars templates see). Two generator options tune the output, read
    // from `generator-options.json.<name>`:
    //   - `include-meta` (default false): keep the reflection `$meta` field.
    //   - `emit-empty`   (default true): keep empty fields and elements; set
    //     false to drop them (see filterValue for the "empty" definition).
    bool const includeMeta = boolOption(config, "include-meta", false);
    bool const emitEmpty = boolOption(config, "emit-empty", true);

    std::string const out = getGeneratorOutputPath(*this, config);
    MRDOCS_TRY(std::string const fileName,
        getSinglePageFullPath(out, fileExtension()));
    return writeToFile(fileName, [&](std::ostream& os) -> Expected<void>
    {
        // Collect symbols by walking from the global namespace (see
        // SymbolCollector) for a deterministic, platform-independent order.
        dom::Array symbols;
        SymbolCollector collect{corpus, symbols, includeMeta, emitEmpty};
        collect(corpus.globalNamespace());
        dom::Object root;
        root.set("symbols", std::move(symbols));
        os << dom::JSON::stringify(dom::Value(std::move(root)));
        return {};
    });
}

} // json

//------------------------------------------------

std::unique_ptr<Generator>
makeJSONGenerator()
{
    return std::make_unique<json::JSONGenerator>();
}

} // mrdocs
