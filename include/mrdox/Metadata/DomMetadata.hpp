//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_DOM_DOMMETADATA_HPP
#define MRDOX_API_DOM_DOMMETADATA_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/Support/Dom.hpp>
#include <mrdox/Metadata.hpp>
#include <type_traits>
#include <memory>

namespace clang {
namespace mrdox {

class MRDOX_DECL
    DomCorpus
{
    struct Impl;

    std::unique_ptr<Impl> impl_;

public:
    Corpus const& corpus;

    ~DomCorpus();

    explicit
    DomCorpus(Corpus const&);

    dom::Object
    get(SymbolID const& id);
};

/** Return a Dom node for the given metadata.
*/
MRDOX_DECL
dom::Object
domCreateInfo(
    SymbolID const& id,
    Corpus const& corpus);

} // mrdox
} // clang

#endif
