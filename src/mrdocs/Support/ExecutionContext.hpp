//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_EXECUTIONCONTEXT_HPP
#define MRDOCS_LIB_SUPPORT_EXECUTIONCONTEXT_HPP

#include <mrdocs/Diagnostics.hpp>
#include <mrdocs/Config.hpp>
#include <mrdocs/detail/Corpus.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <llvm/ADT/SmallString.h>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace mrdocs {

/** A custom execution context for visitation.

    This execution context is similar to
    `clang::tooling::ExecutionContext`.

    It represents the context of an execution,
    including the information about compilation
    and results.

    However, it is customized for the needs of
    MrDocs by referring to the `Config`,
    reporting based on the `Info` and `Diagnostics`
    classes, and including a `results` method
    which returns the `detail::SymbolSet`.

    It stores the `detail::SymbolSet` and `Diagnostics`
    objects and returns them when `results` is called.
*/
class ExecutionContext
{
    Config const& config_;
    std::shared_mutex mutex_;
    Diagnostics diags_;
    detail::SymbolSet info_;
    detail::UndocumentedSymbolSet undocumented_;

public:
    /** Initializes a context

       This function does not take ownership of
       `Config`.

        @param config The configuration to use.
    */
    ExecutionContext(
        Config const& config)
        : config_(config)
    {
    }

    /** Adds symbols and diagnostics to the context.

        This function is called to report the results
        of an execution.

        The `detail::SymbolSet` is merged into the existing
        set of results. Duplicate IDs are merged.

        Any new diagnostics are appended to the
        existing diagnostics and new messages
        are printed to the console.

        @param info The results to report.
        @param diags The diagnostics to report.
    */
    void
    report(
        detail::SymbolSet&& info,
        Diagnostics&& diags,
        detail::UndocumentedSymbolSet&& undocumented);

    /** Called when the execution is complete.

        Report the number of errors and warnings
        in the execution context diagnostics.

        @param level The report level.
    */
    void
    reportEnd(report::Level level);

    /** Returns the results of the execution.

        The results are returned as a set of
        `Info` objects.

        The `detail::SymbolSet` object is moved out of
        the execution context.

        @return The results of the execution.
    */
    mrdocs::Expected<detail::SymbolSet>
    results();

    detail::UndocumentedSymbolSet
    undocumented();
};

} // mrdocs

#endif // MRDOCS_LIB_SUPPORT_EXECUTIONCONTEXT_HPP
