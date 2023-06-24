//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_DOM_DOMCONTEXT_HPP
#define MRDOX_API_DOM_DOMCONTEXT_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Support/Dom.hpp>
#include <string>
#include <unordered_map>

namespace clang {
namespace mrdox {

/** Top-level object passed to templating engine.

    This is often called the "context."
*/
class MRDOX_DECL
    DomContext : public dom::Object
{
public:
    // VFALCO I am using string_view instead of
    // string because heterogeneous comparison does
    // not seem to be working.
    using Hash = std::unordered_map<
        std::string_view, dom::Value>;

    explicit DomContext(Hash hash);
    bool empty() const noexcept override { return false; }
    dom::Value get(std::string_view key) const override;
    std::vector<std::string_view> props() const override;

private:
    Hash hash_;
};

} // mrdox
} // clang

#endif
