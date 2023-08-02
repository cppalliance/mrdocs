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

#ifndef MRDOX_LIB_TOOL_TOOLEXECUTOR_HPP
#define MRDOX_LIB_TOOL_TOOLEXECUTOR_HPP

#include "ExecutionContext.hpp"
#include <mrdox/Config.hpp>
#include <clang/Tooling/CompilationDatabase.h>
#include <optional>

namespace clang {
namespace mrdox {

/** A custom tool executor to run a front-end action.

    This tool executor permits running one action
    on all the TUs in a compilation database, using
    the settings specified in the @ref Config.
*/
class ToolExecutor
{
public:
    ToolExecutor(
        report::Level reportLevel,
        Config const& config,
        tooling::CompilationDatabase const& Compilations);

    constexpr
    report::Level
    getReportLevel() const noexcept
    {
        return reportLevel_;
    }

    ExecutionContext&
    getExecutionContext() noexcept
    {
        return Context;
    };

    Error execute(std::unique_ptr<
        tooling::FrontendActionFactory> Action);

private:
    report::Level reportLevel_;
    Config const& config_;
    tooling::CompilationDatabase const& Compilations;
    ExecutionContext Context;
};

} // mrdox
} // clang

#endif
