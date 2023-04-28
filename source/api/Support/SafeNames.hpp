//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_SUPPORT_SAFENAMES_HPP
#define MRDOX_API_SUPPORT_SAFENAMES_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringMap.h>
#include <string>

namespace clang {
namespace mrdox {

class SafeNames
{
    llvm::StringMap<std::string> map_;

    class Builder;

public:
    explicit
    SafeNames(
        Corpus const&);

    llvm::StringRef
    get(SymbolID const& id) const;

    llvm::StringRef
    get(
        SymbolID const& id,
        char sep,
        std::string& dest) const;

    llvm::StringRef
    getOverload(
        Info const &P,
        llvm::StringRef name,
        char sep,
        std::string& dest) const;
};

} // mrdox
} // clang

#endif
