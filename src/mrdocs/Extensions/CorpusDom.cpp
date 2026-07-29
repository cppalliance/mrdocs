//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "CorpusDom.hpp"
#include "../Metadata/DescribedObjectProxy.hpp"
#include "../Metadata/DocComment/Block/BlockKinds.hpp"
#include "../Metadata/DocComment/Inline/InlineKinds.hpp"
#include "../Metadata/NameKinds.hpp"
#include "../Metadata/Symbol/SymbolKinds.hpp"
#include "../Metadata/TArgKinds.hpp"
#include "../Metadata/TParamKinds.hpp"
#include "../Metadata/TypeKinds.hpp"
#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace mrdocs {

namespace {

// Visit a Symbol's polymorphic kind, wrap the concrete derived type
// in a templated `DescribedObjectProxy`, and return it as a DOM
// value. This is the only entry point that scripts use to see a
// symbol; every other proxy comes from a nested read.
dom::Value
makeSymbolProxyValue(Symbol& sym)
{
    dom::Value result;
    visit(sym, [&](auto& concrete)
    {
        using Concrete = std::remove_cvref_t<decltype(concrete)>;
        result = dom::Value(
            dom::newObject<DescribedObjectProxy<Concrete>>(concrete));
    });
    return result;
}

} // (anon)

dom::Value
buildCorpusDom(Corpus& corpus)
{
    dom::Array symbols;
    for (Symbol const& sym : corpus)
    {
        Symbol* mutableSym = corpus.find(sym.id);
        MRDOCS_ASSERT(mutableSym != nullptr);
        symbols.emplace_back(makeSymbolProxyValue(*mutableSym).getObject());
    }

    // `corpus.get(id)` -- decode the base58 string and look up the
    // matching symbol. Returns a proxy or `null`.
    auto getFn = dom::makeVariadicInvocable(
        [corpusPtr = &corpus]
        (dom::Array const& args) -> dom::Expected<dom::Value>
        {
            if (args.size() < 1 || !args.get(0).isString())
            {
                return Unexpected(dom::Error(
                    "corpus.get: expected a string id"));
            }
            auto const id = fromBase58Str(args.get(0).getString());
            if (!id)
            {
                return dom::Value(nullptr);
            }
            Symbol* mutableSym = corpusPtr->find(*id);
            if (!mutableSym)
            {
                return dom::Value(nullptr);
            }
            return makeSymbolProxyValue(*mutableSym);
        });

    // `corpus.lookup(name)` -- look up a symbol by name in the
    // global namespace, mirroring `Corpus::lookup(name)`.
    auto lookupFn = dom::makeVariadicInvocable(
        [corpusPtr = &corpus]
        (dom::Array const& args) -> dom::Expected<dom::Value>
        {
            if (args.size() < 1 || !args.get(0).isString())
            {
                return Unexpected(dom::Error(
                    "corpus.lookup: expected a string name"));
            }
            std::string_view const nameView = args.get(0).getString();
            Expected<Symbol const&> result = corpusPtr->lookup(
                SymbolID::global, nameView);
            if (!result)
            {
                return dom::Value(nullptr);
            }
            Symbol* mutableSym = corpusPtr->find(result.value().id);
            if (!mutableSym)
            {
                return dom::Value(nullptr);
            }
            return makeSymbolProxyValue(*mutableSym);
        });

    dom::Object corpusObj;
    corpusObj.set("symbols", std::move(symbols));
    corpusObj.set("get", dom::Value(std::move(getFn)));
    corpusObj.set("lookup", dom::Value(std::move(lookupFn)));
    return dom::Value(std::move(corpusObj));
}

dom::Value
buildTransformContext(
    dom::Value const& corpusDom, Config const& config, std::string_view id)
{
    // `ctx.params` is the transform's own `transform-options.<id>` block,
    // keyed by the id it registered under; an empty object when unset.
    auto const& opts = config.transformOptions;
    auto const it = opts.find(std::string(id));
    dom::Object ctx;
    ctx.set("corpus", corpusDom);
    ctx.set("config", dom::Value(config.object()));
    ctx.set("params",
        dom::Value(it != opts.end() ? it->second : dom::Object()));
    return dom::Value(std::move(ctx));
}

} // mrdocs
