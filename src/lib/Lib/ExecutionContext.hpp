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
    const ConfigImpl& config_;

public:
    virtual ~ExecutionContext() = default;

    /** Initializes a context

       This function does not take ownership of
       `ConfigImpl`.

        @param config The configuration to use.
    */
    ExecutionContext(
        const ConfigImpl& config)
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

// ----------------------------------------------------------------

/** An execution context which stores the InfoSet as bitcode.

    It stores the `InfoSet` and `Diagnostics`
    objects, and returns them when `results`
    is called.

    Unlike InfoExecutionContext, which stores
    symbols in an `InfoSet` (a set of `Info`s),
    this class stores the symbols as bitcode.

    Each symbol is converted to bitcode and
    and stored in a map of `SymbolID` to
    a vector of `SmallString`s with the
    bitcodes.

    The execution context will keep all
    unique bitcodes associated with a
    symbol ID.

 */
class BitcodeExecutionContext
    : public ExecutionContext
{
    std::mutex mutex_;
    Diagnostics diags_;

    std::unordered_map<
        SymbolID,
        std::vector<
            llvm::SmallString<0>>> bitcode_;

public:
    using ExecutionContext::ExecutionContext;

    /** Adds symbols and diagnostics to the context.

        This function is called to report the results
        of an execution.

        The `InfoSet` is converted to bitcode and
        merged into the existing set of results.

        If the symbol ID already exists, it checks
        if the exact same bitcode is already present.
        If not, the bitcode is appended to the list
        of bitcodes for that Symbol ID. This means
        duplicate IDs are not merged.

        Any new diagnostics are appended to the
        existing diagnostics and new messages
        are printed to the console.

        @param info The results to report.
        @param diags The diagnostics to report.
    */
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

        The function desearializes the bitcodes
        for each symbol ID and merge them into
        a single `Info` in parallel.

        @return The results of the execution.
     */
    mrdocs::Expected<InfoSet>
    results() override;

    /** Returns the bitcode map.

        The results are returned as a map of
        `SymbolID` to a vector of `SmallString`.

        @return The results of the execution.
    */
    auto& getBitcode()
    {
        return bitcode_;
    }
};


} // mrdocs
} // clang

#endif
