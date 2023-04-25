//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_LIB_ADOC_SAFENAMES_HPP
#define MRDOX_LIB_ADOC_SAFENAMES_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringMap.h>
#include <string>

namespace clang {
namespace mrdox {
namespace adoc {

class SafeNames
{
    llvm::StringMap<std::string> map_;

    class Builder;

public:
    explicit
    SafeNames(
        Corpus const&);

    llvm::StringRef
    get(SymbolID const& id) const noexcept;

    llvm::StringRef
    get(
        std::string& dest,
        SymbolID const& id,
        char sep) const noexcept;

    llvm::StringRef
    getOverload(
        SymbolID const& id);
};

} // adoc
} // mrdox
} // clang

#endif
