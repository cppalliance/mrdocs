//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//

#ifndef MRDOCS_TEST_GOLDEN_SUPPORT_COMPARISON_HPP
#define MRDOCS_TEST_GOLDEN_SUPPORT_COMPARISON_HPP

#include "../Action.hpp"
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <llvm/Support/ErrorOr.h>
#include <string>

namespace mrdocs {

struct TestResults;
class Generator;
class Corpus;
class Config;

namespace test_support {

/** Inputs to one run of @ref generateAndCompareOutput.

    A parameter pack bundling everything the comparison needs. The output
    directory is not here: it is derived from the corpus's config via
    @ref getGeneratorOutputPath (the generator already wrote there). The
    scratch directory's lifetime is owned by the caller, not passed in.

    @li What to render: `gen` (the generator to run) and `corpus` (the
        symbols to feed it; varies per compiler variant, so it is per call).
    @li How to behave: `action`, `writeBad`, `forceUpdate`, command-line
        policy, the same for every test in a run.
    @li Where to report: `filePath` (shown in log messages) and `results`
        (the shared counters the comparison increments).
*/
struct CompareArgs
{
    /// The generator under test. generateAndCompareOutput calls `build()` to
    /// produce the output and `fileExtension()` to name single-page output.
    Generator const& gen;

    /// The extracted symbols to render, passed to `gen.build()`. Built once
    /// per compiler variant, hence supplied per call.
    Corpus const& corpus;

    /// The configuration that drove the build, passed to `gen.build()` and
    /// used to resolve the generator's output directory. A corpus does not
    /// own its config, so it is supplied alongside.
    Config const& config;

    /// The test's input `.cpp` path, used for log messages and to locate the
    /// expected fixture (input path + the generator's extension).
    std::string_view filePath;

    /// Whether to compare against the fixture, create it, or update it.
    Action action;

    /// On a failed single-page comparison under `test`/`create`, also write
    /// the generated document beside the fixture (`*.bad.<ext>`) so it can
    /// be inspected.
    bool writeBad;

    /// Under `update`, rewrite the fixture even when it already matches;
    /// otherwise an unchanged fixture is left untouched.
    bool forceUpdate;

    /// Shared run counters; generateAndCompareOutput bumps "matching" or "written".
    TestResults& results;
};

/** Build the generator's output and compare it against the fixture.

    The generator always writes into the layout's temporary output
    directory. A single-page test then compares the one primary document
    against its expected file; a multipage test compares the whole
    generated tree against the snapshot directory. Both paths share the
    same diff, reporting, and create/update logic.
*/
Expected<void>
generateAndCompareOutput(CompareArgs const& args);

} // namespace test_support
} // namespace mrdocs

#endif // MRDOCS_TEST_GOLDEN_SUPPORT_COMPARISON_HPP
