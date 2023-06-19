//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_DOM_DOMINTERFACE_HPP
#define MRDOX_API_DOM_DOMINTERFACE_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/Metadata/Interface.hpp>
#include <mrdox/Support/Dom.hpp>
#include <memory>

namespace clang {
namespace mrdox {

/** The aggregate interface to a record.
*/
class MRDOX_DECL
    DomInterface : public dom::Object
{
    std::shared_ptr<Interface> I_;
    Corpus const& corpus_;

public:
    DomInterface(
        std::shared_ptr<Interface> I,
        Corpus const& corpus) noexcept;
    dom::Value get(std::string_view key) const override;
    std::vector<std::string_view> props() const override;
};

} // mrdox
} // clang

#endif
