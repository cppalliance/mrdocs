//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Fernando Pelliccioni (fpelliccioni@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_TOOL_STDLIB_HPP
#define MRDOCS_TOOL_STDLIB_HPP

#include <mrdocs/Support/Error.hpp>
#include <llvm/Support/CommandLine.h>
#include <string>

namespace clang {
namespace mrdocs {

/** Set the StdLib directories using the argument as a hint.

    @return The error if any occurred.
*/
Expected<void>
setupStdLibDirs(
    std::vector<std::string>& stdLibDirsArg,
    std::string_view execPath);

} // mrdocs
} // clang

#endif // MRDOCS_TOOL_STDLIB_HPP
