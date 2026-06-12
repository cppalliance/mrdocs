//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//

#ifndef MRDOCS_TEST_SUPPORT_TESTLAYOUT_HPP
#define MRDOCS_TEST_SUPPORT_TESTLAYOUT_HPP

#include <mrdocs/Config.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Support/Expected.hpp>
#include <llvm/ADT/StringRef.h>
#include <string>
#include <memory>

namespace mrdocs {

class ScopedTempDirectory;
enum Action : int;

enum class OutputMode
{
    SinglePage,
    Multipage,
    // Extraction-only run (the no-op generator): no expected output.
    None
};

/** Expected output layout for a single test (single-page or multipage). */
struct TestLayout
{
    OutputMode mode = OutputMode::SinglePage;
    std::string expectedSinglePath;
    std::string multipageRoot;
    std::string multipageFormatRoot;
    std::string generatedOutputRoot;
    std::unique_ptr<ScopedTempDirectory> tempDir;
    bool hasFileConfig = false;
};

struct ResolvedLayout
{
    Config::Settings settings;
    TestLayout layout;
};

/** Settings produced by loadTestSettings before the layout is built.
*/
struct LoadedTestSettings
{
    Config::Settings settings;
    /// True if a per-file mrdocs.yml was found and merged.
    bool hasFileConfig = false;
    /// Snapshot of the directory-level multipage flag before merging.
    /// Used to enforce that multipage may only be enabled at the
    /// per-file level.
    bool dirMultipage = false;
};

/** Load any per-file mrdocs.yml on top of the directory-level settings.

    No layout work is done here: the per-file settings are needed before
    the test's generator is known, so addon discovery can run against
    the merged addons paths.
*/
Expected<LoadedTestSettings>
loadTestSettings(
    llvm::StringRef filePath,
    Config::Settings const& dirSettings,
    ReferenceDirectories const& dirs);

/** Build the per-file layout from already-loaded settings.

    Computes expected-output paths, applies multipage handling (creating
    the temporary output directory and adjusting the settings' output and
    tagfile fields), normalizes the settings, and validates the
    single vs multipage invariants.
*/
Expected<ResolvedLayout>
buildTestLayout(
    llvm::StringRef filePath,
    LoadedTestSettings loaded,
    llvm::StringRef generatorExtension,
    ReferenceDirectories const& dirs,
    Action action);

} // namespace mrdocs

#endif
