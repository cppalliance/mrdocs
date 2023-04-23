//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_GENERATORS_HPP
#define MRDOX_GENERATORS_HPP

#include <mrdox/Platform.hpp>

namespace clang {
namespace mrdox {

/** A dynamic list of Generator
*/
class Generators
{
    Generators() noexcept;

public:
    /** Destructor.
    */
    virtual ~Generators() = default;
};

} // mrdox
} // clang

#endif
