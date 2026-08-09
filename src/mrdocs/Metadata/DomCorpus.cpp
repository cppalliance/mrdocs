//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Corpus.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Metadata/DocComment.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Support/DescribedToDom.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <cstddef>
#include <vector>


namespace mrdocs {

namespace {

// Visit a symbol's polymorphic kind and wrap the concrete derived type
// in a templated DescribedObjectProxy, returned as a DOM value. This is
// the single entry point through which both the generator (`@root.mrdocs
// .corpus`) and extension scripts (`ctx.corpus`) obtain a symbol object;
// every other proxy comes from a nested read. `SymbolT` carries the
// symbol's constness, so a const symbol yields a read-only proxy.
template <class SymbolT>
dom::Value
makeSymbolProxyValue(SymbolT& sym)
{
    dom::Value result;
    visit(sym, [&](auto& concrete)
    {
        using Concrete = std::remove_reference_t<decltype(concrete)>;
        result = dom::Value(
            dom::newObject<DescribedObjectProxy<Concrete>>(concrete));
    });
    return result;
}

// A lazy array view over every symbol in a corpus. It captures only the
// ids up front and materializes a symbol proxy on demand when an element
// is read, so exposing the whole corpus costs nothing until a symbol is
// actually visited. `CorpusT` carries the corpus's constness (a const
// corpus yields read-only proxies).
template <class CorpusT>
class CorpusSymbolsArray final : public dom::ArrayImpl
{
    CorpusT* corpus_;
    std::vector<SymbolID> ids_;

public:
    explicit CorpusSymbolsArray(CorpusT& corpus)
        : corpus_(&corpus)
    {
        for (Symbol const& sym : corpus)
        {
            ids_.push_back(sym.id);
        }
    }

    std::size_t size() const override { return ids_.size(); }

    dom::Value get(std::size_t i) const override
    {
        if (i >= ids_.size())
        {
            return dom::Value(dom::Kind::Undefined);
        }
        auto* sym = corpus_->find(ids_[i]);
        if (!sym)
        {
            return dom::Value(nullptr);
        }
        return makeSymbolProxyValue(*sym);
    }
};

template <class CorpusT>
dom::Value
buildCorpusDomImpl(CorpusT& corpus)
{
    dom::Object corpusObj;

    // `corpus.symbols`: every symbol, materialized lazily on access.
    corpusObj.set("symbols",
        dom::newArray<CorpusSymbolsArray<CorpusT>>(corpus));

    // `corpus.get id`: decode a base58 id string and return that
    // symbol's proxy, or null when the id is malformed or absent.
    corpusObj.set("get", dom::makeVariadicInvocable(
        [corpusPtr = &corpus]
        (dom::Array const& args) -> dom::Expected<dom::Value>
        {
            // A missing or non-string argument is an absent id, so return
            // null rather than erroring. Templates call `corpus.get x` on
            // fields like a template argument's `id`, which is legitimately
            // undefined when the referenced symbol was not extracted (for
            // example a dependency). Erroring there would abort rendering the
            // whole page instead of just omitting a link.
            if (args.size() < 1 || !args.get(0).isString())
            {
                return dom::Value(nullptr);
            }
            auto const id = fromBase58Str(args.get(0).getString());
            if (!id)
            {
                return dom::Value(nullptr);
            }
            auto* sym = corpusPtr->find(*id);
            if (!sym)
            {
                return dom::Value(nullptr);
            }
            return makeSymbolProxyValue(*sym);
        }));

    // `corpus.lookup name`: look up a symbol by qualified name from the
    // global namespace, or null.
    //
    // `corpus.lookup name context`: resolve `name` relative to a scope
    // instead: `context` is either a symbol object (its `id` is used) or a
    // base58 id string. This lets a template resolve a name the way the
    // symbol being rendered would see it, e.g. an unqualified name that
    // names a sibling.
    corpusObj.set("lookup", dom::makeVariadicInvocable(
        [corpusPtr = &corpus]
        (dom::Array const& args) -> dom::Expected<dom::Value>
        {
            if (args.size() < 1 || !args.get(0).isString())
            {
                return Unexpected(dom::Error(
                    "corpus.lookup: expected a string name"));
            }

            // Resolve the optional context argument to a scope id.
            SymbolID context = SymbolID::global;
            if (args.size() >= 2)
            {
                dom::Value const ctx = args.get(1);
                dom::Value const ctxId =
                    ctx.isObject() ? ctx.get("id") : ctx;
                if (ctxId.isString())
                {
                    if (auto const id = fromBase58Str(ctxId.getString()))
                    {
                        context = *id;
                    }
                }
            }

            Expected<Symbol const&> const result =
                corpusPtr->lookup(context, args.get(0).getString());
            if (!result)
            {
                return dom::Value(nullptr);
            }
            auto* sym = corpusPtr->find(result.value().id);
            if (!sym)
            {
                return dom::Value(nullptr);
            }
            return makeSymbolProxyValue(*sym);
        }));

    return {std::move(corpusObj)};
}

} // (anon)

dom::Value
buildCorpusDom(Corpus& corpus)
{
    return buildCorpusDomImpl(corpus);
}

dom::Value
buildCorpusDom(Corpus const& corpus)
{
    return buildCorpusDomImpl(corpus);
}

dom::Value
buildSymbolDom(Symbol const& sym)
{
    return makeSymbolProxyValue(sym);
}

DomCorpus::
~DomCorpus() = default;

DomCorpus::
DomCorpus(Corpus const& corpus)
    : corpus_(corpus)
{
}

Corpus const&
DomCorpus::
getCorpus() const
{
    return corpus_;
}


dom::Object
DomCorpus::
construct(Symbol const& I) const
{
    // See the declaration for what this view is. `visit` downcasts to
    // the concrete symbol type; `dom::ValueFrom` then produces the
    // reflection view for it (see the ValueFrom tag_invoke in
    // DescribedToDom.hpp). `U` is const, so the view is read-only.
    return visit(I, []<class T>(T const& U) -> dom::Object
    {
        return dom::ValueFrom(U).getObject();
    });
}

dom::Value
DomCorpus::
get(SymbolID const& id) const
{
    if (!id)
    {
        return nullptr;
    }
    // VFALCO Hack to deal with symbol IDs
    // being emitted without the corresponding data.
    Symbol const* I = corpus_.find(id);
    MRDOCS_CHECK_OR(I, {});
    return construct(*I);
}

} // mrdocs
