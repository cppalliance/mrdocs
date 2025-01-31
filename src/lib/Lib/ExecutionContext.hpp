//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_TOOL_EXECUTIONCONTEXT_HPP
#define MRDOCS_LIB_TOOL_EXECUTIONCONTEXT_HPP

#include "ConfigImpl.hpp"
#include "Diagnostics.hpp"
#include "Info.hpp"
#include <mrdocs/Support/Error.hpp>
#include <llvm/ADT/SmallString.h>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace clang {
namespace mrdocs {

/** A custom execution context for visitation.

    This execution context is similar to
    `clang::tooling::ExecutionContext`.

    It represents the context of an execution,
    including the information about compilation
    and results.

    However, it is customized for the needs of
    MrDocs by referring to the `ConfigImpl`,
    reporting based on the `Info` and `Diagnostics`
    classes, and including a `results` method
    which returns the `InfoSet`.
*/
class ExecutionContext
{
protected:
    ConfigImpl const& config_;

public:
    virtual ~ExecutionContext() = default;

    /** Initializes a context

       This function does not take ownership of
       `ConfigImpl`.

        @param config The configuration to use.
    */
    ExecutionContext(
        ConfigImpl const& config)
        : config_(config)
    {
    }

    /** Adds symbols and diagnostics to the context.

        This function is called to report the results
        of an execution.

        The `InfoSet` is merged into the existing
        set of results. Duplicate IDs are merged.

        Any new diagnostics are appended to the
        existing diagnostics and new messages
        are printed to the console.

        @param info The results to report.
        @param diags The diagnostics to report.
    */
    virtual
    void
    report(
        InfoSet&& info,
        Diagnostics&& diags) = 0;

    /** Called when the execution is complete.

        Report the number of errors and warnings
        in the execution context diagnostics.

        @param level The report level.
    */
    virtual
    void
    reportEnd(report::Level level) = 0;

    /** Returns the results of the execution.

        The results are returned as a set of
        `Info` objects.

        @return The results of the execution.
    */
    virtual
    mrdocs::Expected<InfoSet>
    results() = 0;
};

// ----------------------------------------------------------------

/** An execution context which stores the InfoSet and Diagnostics.

    It stores the `InfoSet` and `Diagnostics`
    objects, and returns them when `results`
    is called.
 */
class InfoExecutionContext
    : public ExecutionContext
{
    std::shared_mutex mutex_;
    Diagnostics diags_;
    InfoSet info_;

public:
    using ExecutionContext::ExecutionContext;

    /// @copydoc ExecutionContext::report
    void
    report(
        InfoSet&& info,
        Diagnostics&& diags) override;

    /// @copydoc ExecutionContext::reportEnd
    void
    reportEnd(report::Level level) override;

    /** Returns the results of the execution.

        The results are returned as a set of
        `Info` objects.

        The `InfoSet` object is moved out of
        the execution context.

        @return The results of the execution.
    */
    mrdocs::Expected<InfoSet>
    results() override;
};

} // mrdocs
} // clang

#endif
