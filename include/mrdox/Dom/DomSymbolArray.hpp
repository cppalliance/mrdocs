//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_DOM_DOMSYMBOLARRAYHPP
#define MRDOX_API_DOM_DOMSYMBOLARRAYHPP

#include <mrdox/Platform.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/Support/Dom.hpp>
#include <vector>

namespace clang {
namespace mrdox {

/** A vector of symbol IDs.
*/
class MRDOX_DECL
    DomSymbolArray : public dom::Array
{
    std::vector<SymbolID> const& list_;
    Corpus const& corpus_;

public:
    DomSymbolArray(
        std::vector<SymbolID> const& list,
        Corpus const& corpus) noexcept;

    std::size_t length() const noexcept override;
    dom::Value get(std::size_t index) const override;
};

} // mrdox
} // clang

#endif
