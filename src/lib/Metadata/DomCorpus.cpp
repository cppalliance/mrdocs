//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/Dom/LazyObject.hpp>
#include "lib/Support/LegibleNames.hpp"
#include "lib/Support/Radix.hpp"
#include <memory>
#include <mutex>
#include <variant>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>

namespace clang::mrdocs {

class DomCorpus::Impl
{
    using value_type = std::weak_ptr<dom::ObjectImpl>;

    DomCorpus const& domCorpus_;
    Corpus const& corpus_;

public:
    Impl(
        DomCorpus const& domCorpus,
        Corpus const& corpus)
        : domCorpus_(domCorpus)
        , corpus_(corpus)
    {
    }

    Corpus const&
    getCorpus() const
    {
        return corpus_;
    }

    dom::Object
    create(Info const& I) const
    {
        return domCorpus_.construct(I);
    }

    dom::Object
    get(SymbolID const& id) const
    {
        // VFALCO Hack to deal with symbol IDs
        // being emitted without the corresponding data.
        Info const* I = corpus_.find(id);
        MRDOCS_CHECK_OR(I, {});
        return create(*I);
    }
};

DomCorpus::
~DomCorpus() = default;

DomCorpus::
DomCorpus(Corpus const& corpus_)
    : impl_(std::make_unique<Impl>(
        *this, corpus_))
{
}

Corpus const&
DomCorpus::
getCorpus() const
{
    return impl_->getCorpus();
}


dom::Object
DomCorpus::
construct(Info const& I) const
{
    return visit(I, [this]<class T>(T const& U) -> dom::Object
    {
        return dom::ValueFrom(U, this).getObject();
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
    return impl_->get(id);
}

dom::Value
DomCorpus::
getJavadoc(Javadoc const&) const
{
    // Default implementation returns null.
    return nullptr;
}

dom::Array
getParents(DomCorpus const& C, Info const& I)
{
    // A convenient list to iterate over the parents
    // with resorting to partial template recursion
    Corpus const& corpus = C.getCorpus();
    auto const pIds = getParents(corpus, I);
    dom::Array res;
    for (SymbolID const& id : pIds)
    {
        Info const& PI = corpus.get(id);
        res.push_back(C.construct(PI));
    }
    return res;
}

} // clang::mrdocs
