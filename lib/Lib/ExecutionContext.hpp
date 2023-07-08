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

#ifndef MRDOX_TOOL_TOOL_EXECUTIONCONTEXT_HPP
#define MRDOX_TOOL_TOOL_EXECUTIONCONTEXT_HPP

#include "Diagnostics.hpp"
#include <mrdox/Config.hpp>
#include <clang/Tooling/Execution.h>
#include <llvm/Support/Mutex.h>

namespace clang {
namespace mrdox {

/** A custom execution context for visitation.

    This execution context extends the clang base
    class by adding additional state beyond the
    ToolResults, shared by all AST visitor
    instances.
*/
class ExecutionContext
    : public tooling::ExecutionContext
{
    llvm::sys::Mutex mutex_;
    Diagnostics diags_;

public:
    explicit
    ExecutionContext(
        tooling::ToolResults* Results)
            : tooling::ExecutionContext(Results)
    {
    }

    void report(Diagnostics&& diags);
    void reportEnd(report::Level level);
};

} // mrdox
} // clang

#endif
