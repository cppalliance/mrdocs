//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_REPORT_REPORTDOM_HPP
#define MRDOCS_LIB_SUPPORT_REPORT_REPORTDOM_HPP

#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/Reporter.hpp>

namespace mrdocs {

/** Build the `ctx.report` object a script receives.

    The object holds the functions `warn`, `error` and `info`. Each reports
    through `reporter`, the reporter of the run, the one a plugin reaches as
    `ctx.report`. A warning is an error under `warn-as-error`, as it is in a
    plugin. Each function joins its arguments with spaces the way
    `console.log` does, so a script that calls
    `ctx.report.warn("Hello from a transform")` prints and counts the same
    message as a plugin that calls
    `ctx.report->warn("Hello from a transform")`.

    The object refers to `reporter`, which has to outlive it.

    @param reporter The reporter the functions report through.
    @return The `ctx.report` value.
*/
dom::Value
reportDom(report::Reporter& reporter);

} // mrdocs

#endif
