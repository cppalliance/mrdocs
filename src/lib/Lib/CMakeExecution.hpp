//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Fernando Pelliccioni (fpelliccioni@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_TOOL_CMAKE_EXECUTION_HPP
#define MRDOCS_LIB_TOOL_CMAKE_EXECUTION_HPP

#include <string>

#include <llvm/ADT/StringRef.h>
#include <mrdocs/Support/Error.hpp>

namespace clang {
namespace mrdocs {

/**
 * Execute cmake to export compile_commands.json.
*/
Expected<std::string>
executeCmakeExportCompileCommands(llvm::StringRef projectPath);

} // mrdocs
} // clang

#endif // MRDOCS_LIB_TOOL_CMAKE_EXECUTION_HPP

