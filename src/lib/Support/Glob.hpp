//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_GLOB_HPP
#define MRDOCS_LIB_SUPPORT_GLOB_HPP

#include <string>


namespace mrdocs {

/// Check if the string matches the glob pattern
bool
globMatch(
    std::string_view pattern,
    std::string_view str) noexcept;

} // mrdocs


#endif // MRDOCS_LIB_SUPPORT_GLOB_HPP

