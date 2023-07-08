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

#ifndef MRDOX_TOOL_AST_ASTVISITOR_HPP
#define MRDOX_TOOL_AST_ASTVISITOR_HPP

#include "Tool/ConfigImpl.hpp"
#include "Tool/ExecutionContext.hpp"
#include <mrdox/Platform.hpp>
#include <clang/Tooling/Execution.h>
#include <clang/Tooling/Tooling.h>

namespace clang {
namespace mrdox {

/** Return a factory used to create our visitor.
*/
std::unique_ptr<tooling::FrontendActionFactory>
makeFrontendActionFactory(
    tooling::ExecutionContext& ex,
    ConfigImpl const& config);

} // mrdox
} // clang

#endif
