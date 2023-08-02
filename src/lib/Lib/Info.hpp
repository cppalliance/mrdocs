//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_LIB_TOOL_INFO_HPP
#define MRDOX_LIB_TOOL_INFO_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Info.hpp>
#include <memory>
#include <unordered_set>

namespace clang {
namespace mrdox {

struct InfoPtrHasher
{
    using is_transparent = void;

    std::size_t operator()(
        const std::unique_ptr<Info>& I) const;

    std::size_t operator()(
        const SymbolID& id) const;
};

struct InfoPtrEqual
{
    using is_transparent = void;

    bool operator()(
        const std::unique_ptr<Info>& a,
        const std::unique_ptr<Info>& b) const;

    bool operator()(
        const std::unique_ptr<Info>& a,
        const SymbolID& b) const;

    bool operator()(
        const SymbolID& a,
        const std::unique_ptr<Info>& b) const;
};

using InfoSet = std::unordered_set<
    std::unique_ptr<Info>, InfoPtrHasher, InfoPtrEqual>;

} // mrdox
} // clang

#endif
