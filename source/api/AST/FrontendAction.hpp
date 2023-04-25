//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_AST_FRONTENDACTION_HPP
#define MRDOX_API_AST_FRONTENDACTION_HPP

#include <mrdox/Platform.hpp>
#include <clang/Tooling/Execution.h>
#include <clang/Tooling/Tooling.h>
#include <memory>

namespace clang {
namespace mrdox {

class Config;
class Reporter;

/** Return a factory used to create our visitor.
*/
std::unique_ptr<tooling::FrontendActionFactory>
makeFrontendActionFactory(
    tooling::ExecutionContext& exc,
    Config const& config,
    Reporter& R);

} // mrdox
} // clang

#endif
