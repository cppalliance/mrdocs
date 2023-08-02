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

#ifndef MRDOX_LIB_TOOL_EXECUTIONCONTEXT_HPP
#define MRDOX_LIB_TOOL_EXECUTIONCONTEXT_HPP

#include "Diagnostics.hpp"
#include "Info.hpp"
#include <mrdox/Config.hpp>
#include <mrdox/Metadata/Info.hpp>
#include <clang/Tooling/Execution.h>
#include <llvm/ADT/SmallString.h>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace clang {
namespace mrdox {

/** A custom execution context for visitation.

    This execution context extends the clang base
    class by adding additional state beyond the
    ToolResults, shared by all AST visitor
    instances.
*/
class ExecutionContext
{
    std::mutex mutex_;
    Diagnostics diags_;

    std::unordered_map<SymbolID,
        std::vector<SmallString<0>>> bitcode_;
    // InfoSet info_;

public:
#if 0
    explicit
    ExecutionContext(
        tooling::ToolResults* Results)
    {
    }
#endif

    void report(
        InfoSet&& results,
        Diagnostics&& diags);

    auto& getBitcode()
    {
        return bitcode_;
    }

    void reportEnd(report::Level level);
};

} // mrdox
} // clang

#endif
