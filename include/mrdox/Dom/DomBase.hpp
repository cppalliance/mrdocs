//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_DOM_DOMBASE_HPP
#define MRDOX_API_DOM_DOMBASE_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/Dom/DomSymbol.hpp>

namespace clang {
namespace mrdox {

/** A base class record
*/
class MRDOX_DECL
    DomBase : public DomSymbol<RecordInfo>
{
    BaseInfo const* B_;

public:
    DomBase(
        RecordInfo const& I,
        BaseInfo const& B,
        Corpus const& corpus) noexcept;

    dom::Value get(std::string_view key) const override;
    std::vector<std::string_view> props() const override;
};

} // mrdox
} // clang

#endif
