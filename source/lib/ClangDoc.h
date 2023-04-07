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

//
// This file exposes a method to create the FrontendActionFactory for the
// clang-doc tool. The factory runs the clang-doc mapper on a given set of
// source code files, storing the results key-value pairs in its
// ExecutionContext.
//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_DOC_CLANGDOC_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_DOC_CLANGDOC_H

#include <mrdox/Config.hpp>
#include <clang/Tooling/Execution.h>
#include <clang/Tooling/Tooling.h>

namespace clang {
namespace mrdox {

std::unique_ptr<tooling::FrontendActionFactory>
makeToolFactory(
    tooling::ExecutionContext& exc,
    Config const& cfg);

} // mrdox
} // clang

#endif
