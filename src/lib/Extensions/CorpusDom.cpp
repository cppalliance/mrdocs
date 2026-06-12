//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "CorpusDom.hpp"

#include <lib/CorpusImpl.hpp>
#include <lib/Metadata/DocComment/Block/BlockKinds.hpp>
#include <lib/Metadata/DocComment/Inline/InlineKinds.hpp>
#include <lib/Metadata/NameKinds.hpp>
#include <lib/Metadata/Symbol/SymbolKinds.hpp>
#include <lib/Metadata/TArgKinds.hpp>
#include <lib/Metadata/TParamKinds.hpp>
#include <lib/Metadata/TypeKinds.hpp>

#include <mrdocs/Metadata.hpp>
#include <lib/Metadata/DescribedObjectProxy.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Expected.hpp>

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

// Decode a base16-encoded SymbolID string. Returns true on success.
// 40 hex chars (lower or upper) decode into the 20-byte SymbolID.
bool
parseBase16SymbolID(std::string_view s, SymbolID& out)
{
    constexpr std::size_t kIdBytes = 20;
    constexpr std::size_t kHexLen = kIdBytes * 2;
    if (s.size() != kHexLen)
    {
        return false;
    }
    auto const decode = [](char c) -> int
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    SymbolID::value_type bytes[kIdBytes] = {};
    for (std::size_t i = 0; i < kIdBytes; ++i)
    {
        int const hi = decode(s[i * 2]);
        int const lo = decode(s[i * 2 + 1]);
        if (hi < 0 || lo < 0)
        {
            return false;
        }
        bytes[i] = static_cast<SymbolID::value_type>((hi << 4) | lo);
    }
    out = SymbolID(bytes);
    return true;
}

} // (anon)

dom::Value
buildCorpusDom(CorpusImpl& corpus)
{
    dom::Array symbols;
    for (Symbol const& sym : corpus)
    {
        Symbol* mutableSym = corpus.find(sym.id);
        MRDOCS_ASSERT(mutableSym != nullptr);
        symbols.emplace_back(makeSymbolProxyValue(*mutableSym).getObject());
    }

    // `corpus.get(id)` -- decode the base16 string and look up the
    // matching symbol. Returns a proxy or `null`.
    auto getFn = dom::makeVariadicInvocable(
        [corpusPtr = &corpus]
        (dom::Array const& args) -> Expected<dom::Value, Error>
        {
            if (args.size() < 1 || !args.get(0).isString())
            {
                return Unexpected(Error(
                    "corpus.get: expected a string id"));
            }
            SymbolID id;
            if (!parseBase16SymbolID(args.get(0).getString(), id))
            {
                return dom::Value(nullptr);
            }
            Symbol* mutableSym = corpusPtr->find(id);
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
        (dom::Array const& args) -> Expected<dom::Value, Error>
        {
            if (args.size() < 1 || !args.get(0).isString())
            {
                return Unexpected(Error(
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
buildTransformContext(CorpusImpl& corpus)
{
    dom::Object ctx;
    ctx.set("corpus", buildCorpusDom(corpus));
    ctx.set("config", dom::Value(corpus.config.object()));
    return dom::Value(std::move(ctx));
}

} // mrdocs
