//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_SUPPORT_PATH_HPP
#define MRDOX_SUPPORT_PATH_HPP

#include <mrdox/Platform.hpp>
#include <string>
#include <string_view>

namespace clang {
namespace mrdox {

/** Return a full path from a possibly relative path.
*/
std::string
makeFullPath(
    std::string_view pathName,
    std::string_view workingDir);

} // mrdox
} // clang

#endif