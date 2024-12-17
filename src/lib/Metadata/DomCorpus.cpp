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

#include "lib/Support/Radix.hpp"
#include "lib/Support/LegibleNames.hpp"
#include "lib/Dom/LazyObject.hpp"
#include "lib/Dom/LazyArray.hpp"
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <memory>
#include <mutex>
#include <variant>

namespace clang {
namespace mrdocs {

class DomCorpus::Impl
{
    using value_type = std::weak_ptr<dom::ObjectImpl>;

    DomCorpus const& domCorpus_;
    Corpus const& corpus_;
    std::unordered_map<SymbolID, value_type> cache_;
    std::mutex mutex_;

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
    create(Info const& I)
    {
        return domCorpus_.construct(I);
    }

    dom::Object
    get(SymbolID const& id)
    {
        // VFALCO Hack to deal with symbol IDs
        // being emitted without the corresponding data.
        const Info* I = corpus_.find(id);
        if(! I)
            return {}; // VFALCO Hack

        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(id);
        if(it == cache_.end())
        {
            auto obj = create(*I);
            cache_.insert(
                { id, obj.impl() });
            return obj;
        }
        if(auto sp = it->second.lock())
            return dom::Object(sp);
        auto obj = create(*I);
        it->second = obj.impl();
        return obj;
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
    return dom::ValueFrom(I, this).getObject();
}

dom::Object
DomCorpus::
construct(
    OverloadSet const& overloads) const
{
    return dom::ValueFrom(overloads, this).getObject();
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

} // mrdocs
} // clang
