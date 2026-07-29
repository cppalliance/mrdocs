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
#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Metadata/DocComment.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>

namespace mrdocs {

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
    // VFALCO Hack to deal with symbol IDs
    // being emitted without the corresponding data.
    Symbol const* I = corpus_.find(id);
    MRDOCS_CHECK_OR(I, {});
    return construct(*I);
}

dom::Value
DomCorpus::
getDocComment(DocComment const&) const
{
    // Default implementation returns null.
    return nullptr;
}

dom::Array
getParents(DomCorpus const& C, Symbol const& I)
{
    // A convenient list to iterate over the parents
    // with resorting to partial template recursion
    Corpus const& corpus = C.getCorpus();
    auto const pIds = getParents(corpus, I);
    dom::Array res;
    for (SymbolID const& id : pIds)
    {
        Symbol const& PI = corpus.get(id);
        res.push_back(C.construct(PI));
    }
    return res;
}

} // mrdocs
