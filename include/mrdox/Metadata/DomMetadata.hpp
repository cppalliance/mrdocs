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

/** Front-end factory for producing Dom nodes.
*/
class MRDOX_DECL
    DomCorpus
{
    class Impl;

    std::unique_ptr<Impl> impl_;

public:
    Corpus const& corpus;

    ~DomCorpus();

    explicit
    DomCorpus(Corpus const&);

    dom::Object
    get(SymbolID const& id) const;

    dom::Object
    get(Info const& I) const;

    /** Return the object for id, or null if id is zero.
    */
    dom::Value
    getOptional(
        SymbolID const& id) const;
};

} // mrdox
} // clang

#endif
