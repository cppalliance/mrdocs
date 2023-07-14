//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_DOM_KIND_HPP
#define MRDOX_API_DOM_KIND_HPP

#include <mrdox/Platform.hpp>

namespace clang {
namespace mrdox {
namespace dom {

/** The type of data in a Value.
*/
enum class Kind
{
    Null,
    Boolean,
    Integer,
    String,
    Array,
    Object,
    Function
};

} // dom
} // mrdox
} // clang

#endif
