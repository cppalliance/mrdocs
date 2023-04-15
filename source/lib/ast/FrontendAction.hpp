//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_FRONTEND_ACTION_HPP
#define MRDOX_FRONTEND_ACTION_HPP

#include <mrdox/Config.hpp>
#include <mrdox/Reporter.hpp>
#include <clang/Tooling/Execution.h>
#include <clang/Tooling/Tooling.h>
#include <memory>

namespace clang {
namespace mrdox {

/** Return a factory used to visit the AST nodes.
*/
std::unique_ptr<tooling::FrontendActionFactory>
makeFrontendActionFactory(
    tooling::ExecutionContext& exc,
    Config const& config,
    Reporter& R);

} // mrdox
} // clang

#endif
