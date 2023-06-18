//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_DOM_DOMTYPE_HPP
#define MRDOX_API_DOM_DOMTYPE_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/Support/Dom.hpp>

namespace clang {
namespace mrdox {

/** A type
*/
class MRDOX_DECL
    DomType : public dom::Object
{
    TypeInfo const* I_;
    Info const* J_;
    Corpus const& corpus_;

public:
    DomType(
        TypeInfo const& I,
        Corpus const& corpus) noexcept;

    dom::Value get(std::string_view key) const override;
    std::vector<std::string_view> props() const override;
};

} // mrdox
} // clang

#endif
