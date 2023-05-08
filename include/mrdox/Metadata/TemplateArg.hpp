//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_METADATA_TEMPLATEARG_HPP
#define MRDOX_METADATA_TEMPLATEARG_HPP

#include <mrdox/Platform.hpp>
#include <string>

namespace clang {
namespace mrdox {

struct TArg
{
    std::string Value;

    TArg() = default;

    MRDOX_DECL 
    TArg(std::string&& value);
};

} // mrdox
} // clang

#endif
